#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <optional>

#include <ai/ai.h>
#include <ai/logger.h>
#include <ai/openai.h>
#include <ai/tools.h>

extern "C"
{
#include <postgres.h>
#include <fmgr.h>
#include <utils/builtins.h>
#include "utils/guc.h"
#include <executor/spi.h>
#include <catalog/pg_type_d.h>

#ifdef PG_MODULE_MAGIC
    PG_MODULE_MAGIC;
#endif

    // Global configuration variables
    static char *openrouter_api_key = nullptr;
    static char *openrouter_model = nullptr;
    static char *openrouter_base_url = nullptr;

    /**
     * Core function to set memory in database
     * Returns: true on success, false on failure (sets error_msg if provided)
     * manage_spi: if true, handles SPI_connect/finish; if false, uses existing connection
     */
    bool memory_set_core(const std::string &category, const std::string &key,
                         const std::string &value, const std::optional<std::string> &notes,
                         std::string *error_msg = nullptr, bool manage_spi = true)
    {
        std::string sql = "INSERT INTO ai_toolkit.ai_memory (category, key, value, notes, updated_at) "
                          "VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP) "
                          "ON CONFLICT (category, key) DO UPDATE SET "
                          "value = EXCLUDED.value, notes = EXCLUDED.notes, updated_at = CURRENT_TIMESTAMP";

        if (manage_spi && SPI_connect() != SPI_OK_CONNECT)
        {
            if (error_msg)
                *error_msg = "Failed to connect to SPI";
            return false;
        }

        Datum values[4];
        char nulls[4] = {' ', ' ', ' ', notes.has_value() ? ' ' : 'n'};

        values[0] = CStringGetTextDatum(category.c_str());
        values[1] = CStringGetTextDatum(key.c_str());
        values[2] = CStringGetTextDatum(value.c_str());
        if (notes.has_value())
            values[3] = CStringGetTextDatum(notes.value().c_str());

        Oid argtypes[4] = {TEXTOID, TEXTOID, TEXTOID, TEXTOID};
        int ret = SPI_execute_with_args(sql.c_str(), 4, argtypes,
                                        values, nulls, false, 0);

        if (manage_spi)
            SPI_finish();

        if (ret < 0)
        {
            if (error_msg)
                *error_msg = "Failed to execute memory insert";
            return false;
        }

        return true;
    }

    /**
     * Core function to get memory from database
     * Returns: value if found, std::nullopt if not found
     * manage_spi: if true, handles SPI_connect/finish; if false, uses existing connection
     */
    std::optional<std::string> memory_get_core(const std::string &category, const std::string &key,
                                               std::string *error_msg = nullptr, bool manage_spi = true)
    {
        std::string sql = "SELECT value FROM ai_toolkit.ai_memory WHERE category = $1 AND key = $2";

        if (manage_spi && SPI_connect() != SPI_OK_CONNECT)
        {
            if (error_msg)
                *error_msg = "Failed to connect to SPI";
            return std::nullopt;
        }

        Datum values[2];
        char nulls[2] = {' ', ' '};

        values[0] = CStringGetTextDatum(category.c_str());
        values[1] = CStringGetTextDatum(key.c_str());

        Oid argtypes[2] = {TEXTOID, TEXTOID};
        int ret = SPI_execute_with_args(sql.c_str(), 2, argtypes,
                                        values, nulls, true, 1);

        if (ret == SPI_OK_SELECT && SPI_processed > 0)
        {
            bool isnull;
            Datum result_datum = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isnull);

            if (!isnull)
            {
                text *result_text = DatumGetTextPP(result_datum);
                std::string result = std::string(VARDATA_ANY(result_text), VARSIZE_ANY_EXHDR(result_text));
                if (manage_spi)
                    SPI_finish();
                return result;
            }
        }

        if (manage_spi)
            SPI_finish();
        return std::nullopt;
    }

    /**
     * Tool function for AI to set memory
     */
    nlohmann::json tool_set_memory(const nlohmann::json &params, const ai::ToolExecutionContext &context)
    {
        try
        {
            if (!params.contains("category") || !params.contains("key") || !params.contains("value"))
            {
                return nlohmann::json{{"success", false}, {"error", "Missing required parameters: category, key, value"}};
            }

            std::string category = params["category"].get<std::string>();
            std::string key = params["key"].get<std::string>();
            std::string value = params["value"].get<std::string>();
            std::optional<std::string> notes = std::nullopt;

            if (params.contains("notes") && !params["notes"].is_null())
            {
                notes = params["notes"].get<std::string>();
            }

            std::string error_msg;
            bool success = memory_set_core(category, key, value, notes, &error_msg, false);

            if (success)
            {
                return nlohmann::json{{"success", true}, {"message", "Memory saved successfully"}, {"category", category}, {"key", key}};
            }
            else
            {
                return nlohmann::json{{"success", false}, {"error", error_msg}};
            }
        }
        catch (const std::exception &e)
        {
            return nlohmann::json{{"success", false}, {"error", std::string(e.what())}};
        }
    }

    /**
     * Tool function for AI to get memory
     */
    nlohmann::json tool_get_memory(const nlohmann::json &params, const ai::ToolExecutionContext &context)
    {
        try
        {
            if (!params.contains("category") || !params.contains("key"))
            {
                return nlohmann::json{{"success", false}, {"error", "Missing required parameters: category, key"}};
            }

            std::string category = params["category"].get<std::string>();
            std::string key = params["key"].get<std::string>();

            std::string error_msg;
            auto result = memory_get_core(category, key, &error_msg, false);

            if (result.has_value())
            {
                return nlohmann::json{{"success", true}, {"value", result.value()}, {"category", category}, {"key", key}};
            }
            else
            {
                return nlohmann::json{{"success", false}, {"error", "Memory not found"}, {"category", category}, {"key", key}};
            }
        }
        catch (const std::exception &e)
        {
            return nlohmann::json{{"success", false}, {"error", std::string(e.what())}};
        }
    }

    /**
     * Tool function: List all databases in the PostgreSQL instance
     */
    nlohmann::json tool_list_databases(const nlohmann::json &params, const ai::ToolExecutionContext &context)
    {
        elog(LOG, "[tool_list_databases] Starting execution");
        try
        {
            const char *sql = "SELECT datname FROM pg_database WHERE datistemplate = false ORDER BY datname";

            elog(LOG, "[tool_list_databases] About to execute query (using existing SPI connection)");
            int ret = SPI_execute(sql, true, 0);
            elog(LOG, "[tool_list_databases] SPI_execute() returned: %d", ret);

            if (ret != SPI_OK_SELECT)
            {
                elog(LOG, "[tool_list_databases] Query execution failed");
                return nlohmann::json{{"success", false}, {"error", "Failed to query databases"}};
            }

            elog(LOG, "[tool_list_databases] Query successful, processed %lu rows", (unsigned long)SPI_processed);
            nlohmann::json databases = nlohmann::json::array();

            for (uint64 i = 0; i < SPI_processed; i++)
            {
                bool isnull;
                HeapTuple tuple = SPI_tuptable->vals[i];
                TupleDesc tupdesc = SPI_tuptable->tupdesc;
                Datum datum = SPI_getbinval(tuple, tupdesc, 1, &isnull);

                if (!isnull)
                {
                    char *dbname_str = TextDatumGetCString(datum);
                    if (dbname_str)
                    {
                        databases.push_back(std::string(dbname_str));
                        elog(LOG, "[tool_list_databases] Added database: %s", dbname_str);
                    }
                }
            }

            elog(LOG, "[tool_list_databases] Returning result");
            return nlohmann::json{{"success", true}, {"databases", databases}, {"count", databases.size()}};
        }
        catch (const std::exception &e)
        {
            elog(LOG, "[tool_list_databases] Caught std::exception: %s", e.what());
            return nlohmann::json{{"success", false}, {"error", std::string("Exception: ") + e.what()}};
        }
        catch (...)
        {
            elog(LOG, "[tool_list_databases] Caught unknown exception");
            return nlohmann::json{{"success", false}, {"error", "Unknown exception"}};
        }
    }

    /**
     * Tool function: List all tables in a specific database
     */
    nlohmann::json tool_list_tables_in_database(const nlohmann::json &params, const ai::ToolExecutionContext &context)
    {
        elog(LOG, "[tool_list_tables_in_database] Starting execution");
        try
        {
            if (!params.contains("database"))
            {
                elog(LOG, "[tool_list_tables_in_database] Missing database parameter");
                return nlohmann::json{{"success", false}, {"error", "Missing required parameter: database"}};
            }

            std::string database = params["database"].get<std::string>();
            elog(LOG, "[tool_list_tables_in_database] Database parameter: %s", database.c_str());

            // Query to get all tables in the current database
            std::string sql = "SELECT schemaname, tablename FROM pg_tables "
                              "WHERE schemaname NOT IN ('pg_catalog', 'information_schema', 'pg_toast') "
                              "ORDER BY schemaname, tablename";

            elog(LOG, "[tool_list_tables_in_database] About to execute query (using existing SPI connection)");
            int ret = SPI_execute(sql.c_str(), true, 0);
            elog(LOG, "[tool_list_tables_in_database] SPI_execute() returned: %d", ret);

            if (ret != SPI_OK_SELECT)
            {
                elog(LOG, "[tool_list_tables_in_database] Query execution failed");
                return nlohmann::json{{"success", false}, {"error", "Failed to query tables"}};
            }

            elog(LOG, "[tool_list_tables_in_database] Query successful, processed %lu rows", (unsigned long)SPI_processed);
            nlohmann::json tables = nlohmann::json::array();

            for (uint64 i = 0; i < SPI_processed; i++)
            {
                bool isnull;
                HeapTuple tuple = SPI_tuptable->vals[i];
                TupleDesc tupdesc = SPI_tuptable->tupdesc;

                Datum schema_datum = SPI_getbinval(tuple, tupdesc, 1, &isnull);
                Datum table_datum = SPI_getbinval(tuple, tupdesc, 2, &isnull);

                if (!isnull)
                {
                    char *schema_str = TextDatumGetCString(schema_datum);
                    char *table_str = TextDatumGetCString(table_datum);
                    if (schema_str && table_str)
                    {
                        std::string full_name = std::string(schema_str) + "." + std::string(table_str);
                        tables.push_back(full_name);
                        elog(LOG, "[tool_list_tables_in_database] Added table: %s", full_name.c_str());
                    }
                }
            }

            elog(LOG, "[tool_list_tables_in_database] Returning result");
            return nlohmann::json{{"success", true}, {"database", database}, {"tables", tables}, {"count", tables.size()}};
        }
        catch (const std::exception &e)
        {
            elog(LOG, "[tool_list_tables_in_database] Caught std::exception: %s", e.what());
            return nlohmann::json{{"success", false}, {"error", std::string("Exception: ") + e.what()}};
        }
        catch (...)
        {
            elog(LOG, "[tool_list_tables_in_database] Caught unknown exception");
            return nlohmann::json{{"success", false}, {"error", "Unknown exception"}};
        }
    }

    /**
     * Tool function: Get the CREATE TABLE statement (schema) for a specific table
     */
    nlohmann::json tool_get_schema_for_table(const nlohmann::json &params, const ai::ToolExecutionContext &context)
    {
        elog(LOG, "[tool_get_schema_for_table] Starting execution");
        try
        {
            if (!params.contains("table_name"))
            {
                elog(LOG, "[tool_get_schema_for_table] Missing table_name parameter");
                return nlohmann::json{{"success", false}, {"error", "Missing required parameter: table_name"}};
            }

            std::string table_name = params["table_name"].get<std::string>();
            std::string schema_name = "public";

            // Parse schema.table if provided
            size_t dot_pos = table_name.find('.');
            if (dot_pos != std::string::npos)
            {
                schema_name = table_name.substr(0, dot_pos);
                table_name = table_name.substr(dot_pos + 1);
            }

            elog(LOG, "[tool_get_schema_for_table] Schema: %s, Table: %s", schema_name.c_str(), table_name.c_str());

            // Get table columns
            std::string columns_sql =
                "SELECT column_name, data_type, character_maximum_length, "
                "is_nullable, column_default "
                "FROM information_schema.columns "
                "WHERE table_schema = $1 AND table_name = $2 "
                "ORDER BY ordinal_position";

            elog(LOG, "[tool_get_schema_for_table] About to execute query with args (using existing SPI connection)");
            Datum values[2];
            char nulls[2] = {' ', ' '};
            values[0] = CStringGetTextDatum(schema_name.c_str());
            values[1] = CStringGetTextDatum(table_name.c_str());
            Oid argtypes[2] = {TEXTOID, TEXTOID};

            int ret = SPI_execute_with_args(columns_sql.c_str(), 2, argtypes, values, nulls, true, 0);
            elog(LOG, "[tool_get_schema_for_table] SPI_execute_with_args() returned: %d, processed: %lu", ret, (unsigned long)SPI_processed);

            if (ret != SPI_OK_SELECT || SPI_processed == 0)
            {
                elog(LOG, "[tool_get_schema_for_table] Table not found or query failed");
                return nlohmann::json{{"success", false}, {"error", "Table not found or no columns"}};
            }

            // Build CREATE TABLE statement
            std::stringstream create_sql;
            create_sql << "CREATE TABLE " << schema_name << "." << table_name << " (\n";

            nlohmann::json columns = nlohmann::json::array();

            for (uint64 i = 0; i < SPI_processed; i++)
            {
                bool isnull;
                HeapTuple tuple = SPI_tuptable->vals[i];
                TupleDesc tupdesc = SPI_tuptable->tupdesc;

                Datum col_name_datum = SPI_getbinval(tuple, tupdesc, 1, &isnull);
                Datum col_type_datum = SPI_getbinval(tuple, tupdesc, 2, &isnull);
                Datum col_maxlen_datum = SPI_getbinval(tuple, tupdesc, 3, &isnull);
                bool maxlen_isnull = isnull;
                Datum col_nullable_datum = SPI_getbinval(tuple, tupdesc, 4, &isnull);
                Datum col_default_datum = SPI_getbinval(tuple, tupdesc, 5, &isnull);
                bool default_isnull = isnull;

                char *col_name_str = TextDatumGetCString(col_name_datum);
                char *col_type_str = TextDatumGetCString(col_type_datum);
                char *col_nullable_str = TextDatumGetCString(col_nullable_datum);

                if (col_name_str && col_type_str && col_nullable_str)
                {
                    std::string col_name(col_name_str);
                    std::string col_type(col_type_str);
                    std::string col_nullable(col_nullable_str);

                    nlohmann::json col_info;
                    col_info["name"] = col_name;
                    col_info["type"] = col_type;
                    col_info["nullable"] = (col_nullable == "YES");

                    create_sql << "  " << col_name << " " << col_type;

                    if (!maxlen_isnull && col_type == "character varying")
                    {
                        int maxlen = DatumGetInt32(col_maxlen_datum);
                        create_sql << "(" << maxlen << ")";
                    }

                    if (col_nullable == "NO")
                    {
                        create_sql << " NOT NULL";
                    }

                    if (!default_isnull)
                    {
                        char *col_default_str = TextDatumGetCString(col_default_datum);
                        if (col_default_str)
                        {
                            std::string col_default(col_default_str);
                            create_sql << " DEFAULT " << col_default;
                            col_info["default"] = col_default;
                        }
                    }

                    if (i < SPI_processed - 1)
                    {
                        create_sql << ",\n";
                    }
                    columns.push_back(col_info);
                }
            }

            create_sql << "\n);";

            elog(LOG, "[tool_get_schema_for_table] Schema generation complete, returning result");
            return nlohmann::json{
                {"success", true},
                {"table", schema_name + "." + table_name},
                {"create_statement", create_sql.str()},
                {"columns", columns}};
        }
        catch (const std::exception &e)
        {
            elog(LOG, "[tool_get_schema_for_table] Caught std::exception: %s", e.what());
            return nlohmann::json{{"success", false}, {"error", std::string("Exception: ") + e.what()}};
        }
        catch (...)
        {
            elog(LOG, "[tool_get_schema_for_table] Caught unknown exception");
            return nlohmann::json{{"success", false}, {"error", "Unknown exception"}};
        }
    }

    PG_FUNCTION_INFO_V1(help);
    PG_FUNCTION_INFO_V1(set_memory);
    PG_FUNCTION_INFO_V1(get_memory);
    PG_FUNCTION_INFO_V1(query);

    /**
     * Help function - provides toolkit documentation
     */
    Datum help(PG_FUNCTION_ARGS)
    {
        std::string help_text =
            "═══════════════════════════════════════════════════════════════════\n"
            "                    AI TOOLKIT FOR POSTGRESQL                      \n"
            "═══════════════════════════════════════════════════════════════════\n\n"
            "📚 MAIN FUNCTIONS:\n\n"
            "  • ai_toolkit.query(text)  \n"
            "      Generate SQL from natural language with AI assistance\n"
            "      Uses memory system and asks for approval before execution\n"
            "      Example: SELECT ai_toolkit.query('show active users');\n\n"
            "  • ai_toolkit.set_memory(category, key, value, notes)\n"
            "      Store contextual information about database schema\n"
            "      Example: SELECT ai_toolkit.set_memory(\n"
            "          'table', 'users', 'Contains customer data', 'Core table');\n\n"
            "  • ai_toolkit.get_memory(category, key)\n"
            "      Retrieve stored contextual information\n"
            "      Example: SELECT ai_toolkit.get_memory('table', 'users');\n\n"
            "📊 HELPER FUNCTIONS:\n\n"
            "  • ai_toolkit.view_memories()  - View all stored memories\n"
            "  • ai_toolkit.search_memory(keyword)  - Search memories\n"
            "  • ai_toolkit.view_logs(limit)  - View query logs\n\n"
            "⚙️  CONFIGURATION:\n\n"
            "  SET ai_toolkit.openrouter_api_key = 'your-key';\n"
            "  SET ai_toolkit.openrouter_model = 'model-name';\n\n"
            "📖 MEMORY CATEGORIES:\n\n"
            "  table, column, relationship, business_rule, data_pattern,\n"
            "  calculation, permission, custom\n\n"
            "💡 TIP: The AI can autonomously use set_memory and get_memory\n"
            "    during query generation to learn and improve over time!\n\n"
            "For full documentation, visit: github.com/your-repo/ai-toolkit\n"
            "═══════════════════════════════════════════════════════════════════\n";

        PG_RETURN_TEXT_P(cstring_to_text(help_text.c_str()));
    }

    /**
     * Set memory function - PostgreSQL interface
     */
    Datum set_memory(PG_FUNCTION_ARGS)
    {
        text *category_text = PG_GETARG_TEXT_PP(0);
        text *key_text = PG_GETARG_TEXT_PP(1);
        text *value_text = PG_GETARG_TEXT_PP(2);
        text *notes_text = PG_ARGISNULL(3) ? nullptr : PG_GETARG_TEXT_PP(3);

        try
        {
            std::string category(VARDATA_ANY(category_text), VARSIZE_ANY_EXHDR(category_text));
            std::string key(VARDATA_ANY(key_text), VARSIZE_ANY_EXHDR(key_text));
            std::string value(VARDATA_ANY(value_text), VARSIZE_ANY_EXHDR(value_text));
            std::optional<std::string> notes = std::nullopt;

            if (notes_text)
            {
                notes = std::string(VARDATA_ANY(notes_text), VARSIZE_ANY_EXHDR(notes_text));
            }

            std::string error_msg;
            bool success = memory_set_core(category, key, value, notes, &error_msg);

            if (!success)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                         errmsg("Failed to set memory: %s", error_msg.c_str())));
            }

            std::string result = "Memory saved: [" + category + "] " + key;
            PG_RETURN_TEXT_P(cstring_to_text(result.c_str()));
        }
        catch (const std::exception &e)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                     errmsg("Exception in set_memory: %s", e.what())));
        }
    }

    /**
     * Get memory function - PostgreSQL interface
     */
    Datum get_memory(PG_FUNCTION_ARGS)
    {
        text *category_text = PG_GETARG_TEXT_PP(0);
        text *key_text = PG_GETARG_TEXT_PP(1);

        try
        {
            std::string category(VARDATA_ANY(category_text), VARSIZE_ANY_EXHDR(category_text));
            std::string key(VARDATA_ANY(key_text), VARSIZE_ANY_EXHDR(key_text));

            std::string error_msg;
            auto result = memory_get_core(category, key, &error_msg);

            if (result.has_value())
            {
                PG_RETURN_TEXT_P(cstring_to_text(result.value().c_str()));
            }
            else
            {
                PG_RETURN_NULL();
            }
        }
        catch (const std::exception &e)
        {
            ereport(ERROR,
                    (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                     errmsg("Exception in get_memory: %s", e.what())));
        }
    }

    /**
     * Query function - main AI-powered natural language query
     */
    Datum query(PG_FUNCTION_ARGS)
    {
        elog(LOG, "[query] Function starting");
        text *prompt_text = PG_GETARG_TEXT_PP(0);

        try
        {
            if (!openrouter_api_key)
            {
                elog(LOG, "[query] OpenRouter API key not set");
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("ai_toolkit.openrouter_api_key not set")));
            }

            std::string user_prompt(VARDATA_ANY(prompt_text), VARSIZE_ANY_EXHDR(prompt_text));
            std::string api_key(openrouter_api_key);
            std::string base_url = openrouter_base_url ? openrouter_base_url : "https://openrouter.ai/api";
            std::string model = openrouter_model ? std::string(openrouter_model) : "meta-llama/llama-3.2-3b-instruct:free";

            elog(LOG, "[query] User prompt: %s", user_prompt.c_str());
            elog(LOG, "[query] Model: %s", model.c_str());
            elog(LOG, "[query] About to call SPI_connect() in main query function");

            // Connect to SPI for tool functions to use
            if (SPI_connect() != SPI_OK_CONNECT)
            {
                elog(LOG, "[query] SPI_connect() failed in main query function");
                ereport(ERROR,
                        (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                         errmsg("Failed to connect to SPI")));
            }

            elog(LOG, "[query] SPI_connect() successful in main query function");

            // Create AI client
            elog(LOG, "[query] About to create AI client");
            auto client = ai::openai::create_client(api_key, base_url);
            elog(LOG, "[query] AI client created successfully");

            // Define tools for memory operations using helper functions
            elog(LOG, "[query] Defining tools");
            ai::Tool set_memory_tool = ai::create_simple_tool(
                "set_memory",
                "Store information about database schema, tables, columns, relationships, or business rules for future reference. "
                "Parameters: category (table|column|relationship|business_rule|data_pattern|calculation|permission|custom), "
                "key (identifier like table name or 'table.column'), value (information to store), notes (optional context)",
                {{"category", "string"}, {"key", "string"}, {"value", "string"}, {"notes", "string"}},
                tool_set_memory);

            ai::Tool get_memory_tool = ai::create_simple_tool(
                "get_memory",
                "Retrieve previously stored information about database schema, tables, columns, relationships, or business rules. "
                "Parameters: category (table|column|relationship|business_rule|data_pattern|calculation|permission|custom), "
                "key (identifier like table name or 'table.column')",
                {{"category", "string"}, {"key", "string"}},
                tool_get_memory);

            // Define database exploration tools
            ai::Tool list_databases_tool = ai::create_simple_tool(
                "list_databases",
                "List all available databases in the PostgreSQL instance. No parameters required.",
                {},
                tool_list_databases);

            ai::Tool list_tables_tool = ai::create_simple_tool(
                "list_tables_in_database",
                "List all tables in a specific database. Parameters: database (name of the database)",
                {{"database", "string"}},
                tool_list_tables_in_database);

            ai::Tool get_schema_tool = ai::create_simple_tool(
                "get_schema_for_table",
                "Get the CREATE TABLE statement (schema) for a specific table. "
                "Parameters: table_name (name of table, optionally prefixed with schema like 'schema.table')",
                {{"table_name", "string"}},
                tool_get_schema_for_table);

            // Build system prompt with step-by-step process
            std::string system_prompt =
                "You are a PostgreSQL database assistant. Your role is to help users write SELECT queries.\n\n"
                "=== STRICT QUERY RESTRICTIONS ===\n"
                "- ONLY SELECT queries are allowed\n"
                "- NEVER generate DROP, DELETE, UPDATE, or INSERT queries\n"
                "- If user requests data modification operations, respond: 'I can only execute SELECT queries. Data modification operations are not permitted.'\n\n"
                "=== MANDATORY STEP-BY-STEP QUERY GENERATION PROCESS ===\n"
                "You MUST follow these steps in order. DO NOT skip any steps or make assumptions:\n\n"
                "1. MANDATORY: EXPLORE DATABASES FIRST\n"
                "   - ALWAYS start by calling list_databases() to see all available databases\n"
                "   - This is REQUIRED - do not skip this step\n"
                "   - DO NOT assume you know what databases exist\n\n"
                "2. MANDATORY: EXPLORE TABLES IN CURRENT DATABASE\n"
                "   - ALWAYS call list_tables_in_database() to see all available tables\n"
                "   - This is REQUIRED - do not skip this step\n"
                "   - DO NOT assume you know what tables exist\n"
                "   - DO NOT hallucinate table names\n\n"
                "3. MANDATORY: GET TABLE SCHEMAS\n"
                "   - ALWAYS call get_schema_for_table() for ALL tables that might be relevant to the query\n"
                "   - This is REQUIRED - do not skip this step\n"
                "   - DO NOT assume column names or data types\n"
                "   - DO NOT hallucinate column names\n\n"
                "4. CHECK MEMORY FOR ADDITIONAL CONTEXT\n"
                "   - Use get_memory to check for:\n"
                "     * get_memory('table', 'table_name') - table descriptions and usage notes\n"
                "     * get_memory('column', 'table.column') - column details and meanings\n"
                "     * get_memory('relationship', 'table1_table2') - join patterns\n"
                "     * get_memory('business_rule', 'rule_name') - business logic constraints\n"
                "     * get_memory('data_pattern', 'pattern_name') - common data patterns\n"
                "   - Consider any special filtering rules, calculated fields, or data quirks\n\n"
                "5. GENERATE THE QUERY\n"
                "   - Build the SELECT query based ONLY on the information gathered from tools\n"
                "   - Use ONLY table names and columns that were returned by get_schema_for_table\n"
                "   - DO NOT make assumptions or hallucinate schema information\n"
                "   - If you discover new patterns or relationships, use set_memory to save them\n\n"
                "⚠️  CRITICAL: You MUST call list_databases, list_tables_in_database, and get_schema_for_table\n"
                "    for EVERY query. Never skip these steps. Never assume schema information. Never hallucinate.\n\n"
                "=== AVAILABLE TOOLS ===\n"
                "Database exploration:\n"
                "- list_databases() - List all databases in PostgreSQL instance\n"
                "- list_tables_in_database(database) - List all tables in a database\n"
                "- get_schema_for_table(table_name) - Get CREATE TABLE statement for a table\n\n"
                "Memory operations:\n"
                "- get_memory(category, key) - Retrieve stored information\n"
                "- set_memory(category, key, value, notes) - Store information for future use\n\n"
                "Memory categories: table, column, relationship, business_rule, data_pattern, "
                "calculation, permission, custom\n\n"
                "=== RESPONSE FORMAT ===\n"
                "Generate your SQL query ONLY in this exact format:\n"
                "```sql\n"
                "<your SELECT query here>\n"
                "```\n"
                "No other text or explanation is needed.\n";

            // Configure generation options with tools
            ai::GenerateOptions options(model, system_prompt, user_prompt);
            options.tools["set_memory"] = set_memory_tool;
            options.tools["get_memory"] = get_memory_tool;
            options.tools["list_databases"] = list_databases_tool;
            options.tools["list_tables_in_database"] = list_tables_tool;
            options.tools["get_schema_for_table"] = get_schema_tool;
            options.max_steps = 20; // Allow multi-step reasoning with tool calls

            // Add callbacks for intermediate logging
            std::stringstream log_output;

            options.on_step_finish = [&log_output](const ai::GenerateStep &step)
            {
                log_output << "🧠 thinking";
                if (!step.text.empty())
                {
                    // Show a snippet of the thinking
                    std::string snippet = step.text;
                    log_output << ": " << snippet;
                }
                log_output << "\n";

                elog(NOTICE, "%s", log_output.str().c_str());
                log_output.str("");
                log_output.clear();
            };

            options.on_tool_call_start = [&log_output](const ai::ToolCall &call)
            {
                log_output << "🔧 Calling: " << call.tool_name;
                if (!call.id.empty())
                {
                    // Show truncated ID
                    log_output << " [" << call.id << "]";
                }

                // Show arguments if they exist
                if (!call.arguments.empty() && !call.arguments.is_null())
                {
                    log_output << "\n  └─ Args: " << call.arguments.dump();
                }
                log_output << "\n";

                elog(NOTICE, "%s", log_output.str().c_str());
                log_output.str("");
                log_output.clear();
            };

            options.on_tool_call_finish = [&log_output](const ai::ToolResult &result)
            {
                log_output << "✓ " << result.tool_name << " completed\n";

                // Show a summary of the result
                if (!result.result.empty() && !result.result.is_null())
                {
                    if (result.result.contains("success") && result.result["success"] == true)
                    {
                        // Format based on tool type
                        if (result.tool_name == "list_databases" && result.result.contains("count"))
                        {
                            int count = result.result["count"];
                            std::string preview;
                            if (result.result.contains("databases") && result.result["databases"].is_array())
                            {
                                auto dbs = result.result["databases"];
                                int show = std::min(5, (int)dbs.size());
                                for (int i = 0; i < show; i++)
                                {
                                    if (i > 0)
                                        preview += " - ";
                                    preview += dbs[i].get<std::string>();
                                }
                                if (dbs.size() > 5)
                                    preview += "...";
                            }
                            log_output << "  └─ Found " << count << " databases: " << preview << "\n";
                        }
                        else if (result.tool_name == "list_tables_in_database" && result.result.contains("count"))
                        {
                            int count = result.result["count"];
                            std::string db = result.result.value("database", "");
                            std::string preview;
                            if (result.result.contains("tables") && result.result["tables"].is_array())
                            {
                                auto tables = result.result["tables"];
                                int show = std::min(5, (int)tables.size());
                                for (int i = 0; i < show; i++)
                                {
                                    if (i > 0)
                                        preview += " - ";
                                    preview += tables[i].get<std::string>();
                                }
                                if (tables.size() > 5)
                                    preview += "...";
                            }
                            log_output << "  └─ Found " << count << " tables in database '" << db << "': " << preview << "\n";
                        }
                        else if (result.tool_name == "get_schema_for_table" && result.result.contains("table"))
                        {
                            std::string table = result.result["table"];
                            int col_count = result.result.contains("columns") ? result.result["columns"].size() : 0;
                            log_output << "  └─ Retrieved schema for '" << table << "' (" << col_count << " columns)\n";
                        }
                        else if (result.tool_name == "set_memory")
                        {
                            std::string category = result.result.value("category", "");
                            std::string key = result.result.value("key", "");
                            log_output << "  └─ Saved memory: [" << category << "] " << key << "\n";
                        }
                        else if (result.tool_name == "get_memory")
                        {
                            std::string category = result.result.value("category", "");
                            std::string key = result.result.value("key", "");
                            if (result.result.contains("value"))
                            {
                                log_output << "  └─ Retrieved memory: [" << category << "] " << key << "\n";
                            }
                            else
                            {
                                log_output << "  └─ No memory found: [" << category << "] " << key << "\n";
                            }
                        }
                        else
                        {
                            log_output << "  └─ Success\n";
                        }
                    }
                    else
                    {
                        std::string error = result.result.value("error", "Unknown error");
                        log_output << "  └─ Error: " << error << "\n";
                    }
                }

                elog(NOTICE, "%s", log_output.str().c_str());
                log_output.str("");
                log_output.clear();
            };

            // Generate response
            elog(LOG, "[query] About to call client.generate_text()");
            auto result = client.generate_text(options);
            elog(LOG, "[query] client.generate_text() completed");

            if (result)
            {
                // Parse SQL query from response
                std::string response_text = result.text;
                std::string sql_query;

                // Look for ```sql ... ``` pattern
                size_t sql_start = response_text.find("```sql");
                if (sql_start != std::string::npos)
                {
                    sql_start += 6; // Move past "```sql"
                    size_t sql_end = response_text.find("```", sql_start);
                    if (sql_end != std::string::npos)
                    {
                        sql_query = response_text.substr(sql_start, sql_end - sql_start);
                        // Trim whitespace
                        size_t first = sql_query.find_first_not_of(" \n\r\t");
                        size_t last = sql_query.find_last_not_of(" \n\r\t");
                        if (first != std::string::npos && last != std::string::npos)
                        {
                            sql_query = sql_query.substr(first, last - first + 1);
                        }
                    }
                }

                if (!sql_query.empty())
                {
                    // Execute the SQL query
                    elog(NOTICE, "\n📋 Generated Query:\n%s\n", sql_query.c_str());
                    elog(LOG, "[query] About to execute generated SQL query");

                    // SPI is already connected from the beginning of the function
                    int ret = SPI_execute(sql_query.c_str(), true, 0);
                    elog(LOG, "[query] SQL query execution returned: %d", ret);

                    if (ret < 0)
                    {
                        elog(LOG, "[query] SQL query execution failed");
                        SPI_finish();
                        ereport(ERROR,
                                (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                                 errmsg("Query execution failed")));
                    }

                    elog(LOG, "[query] SQL query executed successfully");
                    // Print results as a table
                    if (SPI_processed > 0)
                    {
                        std::stringstream table_output;
                        table_output << "\n📊 Query Results (" << SPI_processed << " rows):\n";
                        table_output << "═══════════════════════════════════════════════════════════\n";

                        SPITupleTable *tuptable = SPI_tuptable;
                        TupleDesc tupdesc = tuptable->tupdesc;

                        // Print column headers
                        for (int i = 0; i < tupdesc->natts; i++)
                        {
                            if (i > 0)
                                table_output << " | ";
                            table_output << SPI_fname(tupdesc, i + 1);
                        }
                        table_output << "\n";
                        table_output << "───────────────────────────────────────────────────────────\n";

                        // Print rows
                        for (uint64 row = 0; row < SPI_processed; row++)
                        {
                            HeapTuple tuple = tuptable->vals[row];
                            for (int col = 1; col <= tupdesc->natts; col++)
                            {
                                if (col > 1)
                                    table_output << " | ";

                                bool isnull;
                                Datum datum = SPI_getbinval(tuple, tupdesc, col, &isnull);

                                if (isnull)
                                {
                                    table_output << "NULL";
                                }
                                else
                                {
                                    char *value = SPI_getvalue(tuple, tupdesc, col);
                                    table_output << value;
                                    pfree(value);
                                }
                            }
                            table_output << "\n";
                        }

                        table_output << "═══════════════════════════════════════════════════════════\n";
                        elog(NOTICE, "%s", table_output.str().c_str());
                    }
                    else
                    {
                        elog(NOTICE, "\n✓ Query executed successfully. No rows returned.\n");
                    }

                    elog(LOG, "[query] About to call SPI_finish() before return");
                    SPI_finish();
                    elog(LOG, "[query] SPI_finish() successful, returning void");
                    PG_RETURN_VOID();
                }
                else
                {
                    elog(LOG, "[query] No SQL query found in response");
                    SPI_finish();
                    ereport(ERROR,
                            (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                             errmsg("No SQL query found in response. Expected format: ```sql <query> ```")));
                }
            }
            else
            {
                std::string error_msg = "AI query failed: " + result.error_message();
                if (result.error.has_value())
                {
                    error_msg += " | " + result.error.value();
                }

                elog(LOG, "[query] AI generation failed: %s", error_msg.c_str());
                SPI_finish();
                ereport(ERROR,
                        (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                         errmsg("%s", error_msg.c_str())));
            }
        }
        catch (const std::exception &e)
        {
            elog(LOG, "[query] Caught exception in main query function: %s", e.what());
            SPI_finish();
            ereport(ERROR,
                    (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                     errmsg("Exception in query: %s", e.what())));
        }
    }
}

extern "C"
{
    void _PG_init(void)
    {
        DefineCustomStringVariable("ai_toolkit.openrouter_api_key",
                                   "OpenRouter API Key",
                                   "API key for OpenRouter service",
                                   &openrouter_api_key,
                                   nullptr,
                                   PGC_SUSET,
                                   0,
                                   nullptr,
                                   nullptr,
                                   nullptr);

        DefineCustomStringVariable("ai_toolkit.openrouter_model",
                                   "OpenRouter Model",
                                   "Model to use (default: meta-llama/llama-3.2-3b-instruct:free)",
                                   &openrouter_model,
                                   "meta-llama/llama-3.2-3b-instruct:free",
                                   PGC_USERSET,
                                   0,
                                   nullptr,
                                   nullptr,
                                   nullptr);

        DefineCustomStringVariable("ai_toolkit.openrouter_base_url",
                                   "OpenRouter Base URL",
                                   "Base URL for OpenRouter API",
                                   &openrouter_base_url,
                                   "https://openrouter.ai/api",
                                   PGC_USERSET,
                                   0,
                                   nullptr,
                                   nullptr,
                                   nullptr);

        ereport(LOG, (errmsg("ai_toolkit extension loaded")));
    }

    void _PG_fini(void)
    {
        ereport(LOG, (errmsg("ai_toolkit extension unloaded")));
    }
}

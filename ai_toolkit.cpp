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
     * Tool function: List all schemas in the PostgreSQL database
     */
    nlohmann::json tool_list_schemas(const nlohmann::json &params, const ai::ToolExecutionContext &context)
    {
        // Check SPI connection state
        if (SPI_connect() == SPI_ERROR_CONNECT)
        {
            // SPI already connected (expected in tool context) - not an error
        }
        else
        {
            SPI_finish();
        }

        const char *sql = "SELECT schema_name FROM information_schema.schemata "
                          "WHERE schema_name NOT IN ('pg_catalog', 'information_schema', 'pg_toast', 'pg_toast_temp_1') "
                          "ORDER BY schema_name";

        int ret = SPI_execute(sql, true, 0);

        if (ret != SPI_OK_SELECT)
        {
            elog(WARNING, "[tool_list_schemas] Query failed with code: %d", ret);
            nlohmann::json error_result;
            error_result["success"] = false;
            error_result["error"] = "Failed to query schemas";
            return error_result;
        }

        uint64 row_count = SPI_processed;

        // Use vector first, then convert to json at the end
        std::vector<std::string> schema_list;
        schema_list.reserve(row_count);

        for (uint64 i = 0; i < row_count; i++)
        {
            bool isnull = false;
            HeapTuple tuple = SPI_tuptable->vals[i];
            TupleDesc tupdesc = SPI_tuptable->tupdesc;

            Datum schema_datum = SPI_getbinval(tuple, tupdesc, 1, &isnull);

            if (isnull)
            {
                elog(WARNING, "[tool_list_schemas] Skipping NULL schema name at row %lu", (unsigned long)i);
                continue;
            }

            // Use SPI_getvalue instead of TextDatumGetCString - it's safer
            char *schema_name_cstr = SPI_getvalue(tuple, tupdesc, 1);

            if (schema_name_cstr == NULL)
            {
                elog(WARNING, "[tool_list_schemas] Failed to get schema name at row %lu", (unsigned long)i);
                continue;
            }

            // Create string and immediately free
            std::string schema_name(schema_name_cstr);
            pfree(schema_name_cstr);

            if (!schema_name.empty())
            {
                schema_list.push_back(schema_name);
            }
        }

        // Build JSON response
        nlohmann::json result;
        result["success"] = true;
        result["count"] = schema_list.size();
        result["schemas"] = schema_list;

        elog(LOG, "[tool_list_schemas] Retrieved %lu schemas", (unsigned long)schema_list.size());
        return result;
    }

    /**
     * Tool function: List all tables in a specific schema
     */
    nlohmann::json tool_list_tables_in_schema(const nlohmann::json &params, const ai::ToolExecutionContext &context)
    {
        if (!params.contains("schema"))
        {
            elog(WARNING, "[tool_list_tables_in_schema] Missing schema parameter");
            return nlohmann::json{{"success", false}, {"error", "Missing required parameter: schema"}};
        }

        std::string schema = params["schema"].get<std::string>();

        // Query tables from a specific schema using information_schema
        // The table_schema column in information_schema.tables contains the schema name
        std::string sql = "SELECT table_schema, table_name FROM information_schema.tables "
                          "WHERE table_schema = $1 "
                          "AND table_type = 'BASE TABLE' "
                          "ORDER BY table_schema, table_name";

        Datum values[1];
        char nulls[1] = {' '};
        values[0] = CStringGetTextDatum(schema.c_str());
        Oid argtypes[1] = {TEXTOID};

        int ret = SPI_execute_with_args(sql.c_str(), 1, argtypes, values, nulls, true, 0);

        if (ret != SPI_OK_SELECT)
        {
            elog(WARNING, "[tool_list_tables_in_schema] Query execution failed with code: %d for schema: %s", ret, schema.c_str());
            return nlohmann::json{{"success", false}, {"error", "Failed to query tables"}};
        }

        nlohmann::json tables = nlohmann::json::array();

        for (uint64 i = 0; i < SPI_processed; i++)
        {
            bool isnull_schema, isnull_table;
            HeapTuple tuple = SPI_tuptable->vals[i];
            TupleDesc tupdesc = SPI_tuptable->tupdesc;

            Datum schema_datum = SPI_getbinval(tuple, tupdesc, 1, &isnull_schema);
            Datum table_datum = SPI_getbinval(tuple, tupdesc, 2, &isnull_table);

            if (isnull_schema || isnull_table)
            {
                elog(WARNING, "[tool_list_tables_in_schema] Skipping row with NULL schema or table");
                continue;
            }

            // Properly handle memory
            char *schema_cstr = TextDatumGetCString(schema_datum);
            char *table_cstr = TextDatumGetCString(table_datum);

            if (schema_cstr == NULL || table_cstr == NULL)
            {
                elog(WARNING, "[tool_list_tables_in_schema] Failed to get string values from datum");
                if (schema_cstr)
                    pfree(schema_cstr);
                if (table_cstr)
                    pfree(table_cstr);
                continue;
            }

            // Copy to std::string
            std::string schema_str(schema_cstr);
            std::string table_str(table_cstr);

            // Free memory
            pfree(schema_cstr);
            pfree(table_cstr);

            if (!schema_str.empty() && !table_str.empty())
            {
                std::string full_name = schema_str + "." + table_str;
                tables.push_back(full_name);
            }
        }

        elog(LOG, "[tool_list_tables_in_schema] Retrieved %lu tables from schema '%s'", (unsigned long)tables.size(), schema.c_str());
        return nlohmann::json{{"success", true}, {"schema", schema}, {"tables", tables}, {"count", tables.size()}};
    }

    /**
     * Tool function: Get the CREATE TABLE statement (schema) for a specific table
     */
    nlohmann::json tool_get_schema_for_table(const nlohmann::json &params, const ai::ToolExecutionContext &context)
    {
        if (!params.contains("table_name"))
        {
            elog(WARNING, "[tool_get_schema_for_table] Missing table_name parameter");
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

        std::string columns_sql =
            "SELECT column_name, data_type, character_maximum_length, "
            "is_nullable, column_default "
            "FROM information_schema.columns "
            "WHERE table_schema = $1 AND table_name = $2 "
            "ORDER BY ordinal_position";

        Datum values[2];
        char nulls[2] = {' ', ' '};
        values[0] = CStringGetTextDatum(schema_name.c_str());
        values[1] = CStringGetTextDatum(table_name.c_str());
        Oid argtypes[2] = {TEXTOID, TEXTOID};

        int ret = SPI_execute_with_args(columns_sql.c_str(), 2, argtypes, values, nulls, true, 0);

        if (ret != SPI_OK_SELECT || SPI_processed == 0)
        {
            elog(WARNING, "[tool_get_schema_for_table] Table '%s.%s' not found or no columns", schema_name.c_str(), table_name.c_str());
            return nlohmann::json{{"success", false}, {"error", "Table not found or no columns"}};
        }

        std::stringstream create_sql;
        create_sql << "CREATE TABLE " << schema_name << "." << table_name << " (\n";

        nlohmann::json columns = nlohmann::json::array();

        for (uint64 i = 0; i < SPI_processed; i++)
        {
            bool isnull;
            HeapTuple tuple = SPI_tuptable->vals[i];
            TupleDesc tupdesc = SPI_tuptable->tupdesc;

            // Get all column attributes
            Datum col_name_datum = SPI_getbinval(tuple, tupdesc, 1, &isnull);
            if (isnull)
                continue;

            Datum col_type_datum = SPI_getbinval(tuple, tupdesc, 2, &isnull);
            if (isnull)
                continue;

            Datum col_maxlen_datum = SPI_getbinval(tuple, tupdesc, 3, &isnull);
            bool maxlen_isnull = isnull;

            Datum col_nullable_datum = SPI_getbinval(tuple, tupdesc, 4, &isnull);
            if (isnull)
                continue;

            Datum col_default_datum = SPI_getbinval(tuple, tupdesc, 5, &isnull);
            bool default_isnull = isnull;

            // Properly handle memory
            char *col_name_cstr = TextDatumGetCString(col_name_datum);
            char *col_type_cstr = TextDatumGetCString(col_type_datum);
            char *col_nullable_cstr = TextDatumGetCString(col_nullable_datum);

            if (!col_name_cstr || !col_type_cstr || !col_nullable_cstr)
            {
                elog(WARNING, "[tool_get_schema_for_table] Failed to retrieve column information");
                if (col_name_cstr)
                    pfree(col_name_cstr);
                if (col_type_cstr)
                    pfree(col_type_cstr);
                if (col_nullable_cstr)
                    pfree(col_nullable_cstr);
                continue;
            }

            // Copy to std::string
            std::string col_name(col_name_cstr);
            std::string col_type(col_type_cstr);
            std::string col_nullable(col_nullable_cstr);

            // Free immediately after copying
            pfree(col_name_cstr);
            pfree(col_type_cstr);
            pfree(col_nullable_cstr);

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
                char *col_default_cstr = TextDatumGetCString(col_default_datum);
                if (col_default_cstr)
                {
                    std::string col_default(col_default_cstr);
                    pfree(col_default_cstr);

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

        create_sql << "\n);";

        elog(LOG, "[tool_get_schema_for_table] Retrieved schema for '%s.%s' with %lu columns", schema_name.c_str(), table_name.c_str(), (unsigned long)columns.size());
        return nlohmann::json{
            {"success", true},
            {"table", schema_name + "." + table_name},
            {"create_statement", create_sql.str()},
            {"columns", columns}};
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
        text *prompt_text = PG_GETARG_TEXT_PP(0);

        try
        {
            if (!openrouter_api_key)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("ai_toolkit.openrouter_api_key not set")));
            }

            std::string user_prompt(VARDATA_ANY(prompt_text), VARSIZE_ANY_EXHDR(prompt_text));
            std::string api_key(openrouter_api_key);
            std::string base_url = openrouter_base_url ? openrouter_base_url : "https://openrouter.ai/api";
            std::string model = openrouter_model ? std::string(openrouter_model) : "meta-llama/llama-3.2-3b-instruct:free";

            // Connect to SPI for tool functions to use
            if (SPI_connect() != SPI_OK_CONNECT)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                         errmsg("Failed to connect to SPI")));
            }

            // Create AI client
            auto client = ai::openai::create_client(api_key, base_url);

            // Define tools for memory operations using helper functions
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
            ai::Tool list_schemas_tool = ai::create_simple_tool(
                "list_schemas",
                "List all available schemas in the current PostgreSQL database. "
                "Schemas: users (user data), products (catalog), cart (shopping), coupon (discounts), "
                "wallet (payments), orders (order mgmt), payments (transactions), ai_toolkit (system). No parameters required.",
                {},
                tool_list_schemas);

            ai::Tool list_tables_tool = ai::create_simple_tool(
                "list_tables_in_schema",
                "List all tables in a specific schema. Parameters: schema (name of the schema like 'users', 'products', 'orders', etc.)",
                {{"schema", "string"}},
                tool_list_tables_in_schema);

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
                "1. MANDATORY: EXPLORE SCHEMAS FIRST\n"
                "   - ALWAYS start by calling list_schemas() to see all available schemas\n"
                "   - This is REQUIRED - do not skip this step\n"
                "   - DO NOT assume you know what schemas exist\n"
                "2. MANDATORY: EXPLORE TABLES IN RELEVANT SCHEMA\n"
                "   - ALWAYS call list_tables_in_schema() for each relevant schema\n"
                "   - This is REQUIRED - do not skip this step\n"
                "   - DO NOT assume you know what tables exist in a schema\n"
                "   - DO NOT hallucinate table names\n\n"
                "3. MANDATORY: GET TABLE SCHEMAS\n"
                "   - ALWAYS call get_schema_for_table() for ALL tables that might be relevant to the query\n"
                "   - This is REQUIRED - do not skip this step\n"
                "   - Use the fully qualified name: schema.table (e.g., 'users.users', 'products.products')\n"
                "   - DO NOT assume column names or data types\n"
                "   - DO NOT hallucinate column names\n\n"
                "4. CHECK MEMORY FOR ADDITIONAL CONTEXT\n"
                "   - Use get_memory to check for:\n"
                "     * get_memory('table', 'schema.table_name') - table descriptions and usage notes\n"
                "     * get_memory('column', 'schema.table.column') - column details and meanings\n"
                "     * get_memory('relationship', 'table1_table2') - join patterns\n"
                "     * get_memory('business_rule', 'rule_name') - business logic constraints\n"
                "     * get_memory('data_pattern', 'pattern_name') - common data patterns\n"
                "   - Consider any special filtering rules, calculated fields, or data quirks\n\n"
                "5. GENERATE THE QUERY\n"
                "   - Build the SELECT query based ONLY on the information gathered from tables and schema tools\n"
                "   - Don't worry if you don't have context from the memory you co-relate based on the table structure information\n"
                "   - Use ONLY table names and columns that were returned by get_schema_for_table\n"
                "   - Use schema-qualified names in your query (e.g., 'users.users', 'orders.orders')\n"
                "   - DO NOT make assumptions or hallucinate schema information\n"
                "   - If you discover new patterns or relationships, use set_memory to save them\n\n"
                "⚠️  CRITICAL: You MUST call list_tables_in_schema and get_schema_for_table\n"
                "    for EVERY query. Never skip these steps. Never assume schema information. Never hallucinate.\n\n"
                "=== AVAILABLE TOOLS ===\n"
                "Schema exploration:\n"
                "- list_schemas() - List all available schemas in the current database\n"
                "- list_tables_in_schema(schema) - List all tables in a specific schema\n"
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
            options.tools["list_schemas"] = list_schemas_tool;
            options.tools["list_tables_in_schema"] = list_tables_tool;
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
                        if (result.tool_name == "list_schemas" && result.result.contains("count"))
                        {
                            int count = result.result["count"];
                            std::string preview;
                            if (result.result.contains("schemas") && result.result["schemas"].is_array())
                            {
                                auto schemas = result.result["schemas"];
                                int show = std::min(5, (int)schemas.size());
                                for (int i = 0; i < show; i++)
                                {
                                    if (i > 0)
                                        preview += " - ";
                                    preview += schemas[i].get<std::string>();
                                }
                                if (schemas.size() > 5)
                                    preview += "...";
                            }
                            log_output << "  └─ Found " << count << " schemas: " << preview << "\n";
                        }
                        else if (result.tool_name == "list_tables_in_schema" && result.result.contains("count"))
                        {
                            int count = result.result["count"];
                            std::string schema = result.result.value("schema", "");
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
                            log_output << "  └─ Found " << count << " tables in schema '" << schema << "': " << preview << "\n";
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
            auto result = client.generate_text(options);

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

                    // SPI is already connected from the beginning of the function
                    int ret = SPI_execute(sql_query.c_str(), true, 0);

                    if (ret < 0)
                    {
                        SPI_finish();
                        ereport(ERROR,
                                (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                                 errmsg("Query execution failed")));
                    }

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

                    SPI_finish();
                    PG_RETURN_VOID();
                }
                else
                {
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

                SPI_finish();
                ereport(ERROR,
                        (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                         errmsg("%s", error_msg.c_str())));
            }
        }
        catch (const std::exception &e)
        {
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

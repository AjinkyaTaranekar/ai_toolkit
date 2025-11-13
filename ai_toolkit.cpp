#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <optional>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <vector>

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
#include <utils/elog.h>

#ifdef PG_MODULE_MAGIC
    PG_MODULE_MAGIC;
#endif

    // Global configuration variables
    static char *openrouter_api_key = nullptr;
    static char *openrouter_model = nullptr;
    static char *openrouter_base_url = nullptr;
    static char *prompt_file_path = nullptr;

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
     * Utility function to load system prompt from file
     * Returns: prompt string from file if successful, or default hardcoded prompt if file cannot be read
     */
    std::string load_system_prompt()
    {
        // Default hardcoded prompt (used as fallback)
        std::string default_prompt =
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
            "<sql>\n"
            "<your SELECT query here>\n"
            "</sql>\n"
            "No other text or explanation is needed.\n";

        // If no prompt file is configured, use default
        if (!prompt_file_path || strlen(prompt_file_path) == 0)
        {
            elog(LOG, "[load_system_prompt] No prompt file configured, using default prompt");
            return default_prompt;
        }

        try
        {
            std::filesystem::path file_path(prompt_file_path);

            // Check if file exists
            if (!std::filesystem::exists(file_path))
            {
                elog(WARNING, "[load_system_prompt] Prompt file not found at '%s', using default prompt", prompt_file_path);
                return default_prompt;
            }

            // Read file contents
            std::ifstream file(file_path);
            if (!file.is_open())
            {
                elog(WARNING, "[load_system_prompt] Failed to open prompt file at '%s', using default prompt", prompt_file_path);
                return default_prompt;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();

            std::string file_content = buffer.str();

            if (file_content.empty())
            {
                elog(WARNING, "[load_system_prompt] Prompt file is empty at '%s', using default prompt", prompt_file_path);
                return default_prompt;
            }

            elog(LOG, "[load_system_prompt] Successfully loaded prompt from '%s' (%zu bytes)", prompt_file_path, file_content.length());
            return file_content;
        }
        catch (const std::exception &e)
        {
            elog(WARNING, "[load_system_prompt] Exception while reading prompt file: %s, using default prompt", e.what());
            return default_prompt;
        }
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
            char *schema_cstr = SPI_getvalue(tuple, tupdesc, 1);
            char *table_cstr = SPI_getvalue(tuple, tupdesc, 2);

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
            char *col_name_cstr = SPI_getvalue(tuple, tupdesc, 1);
            char *col_type_cstr = SPI_getvalue(tuple, tupdesc, 2);
            char *col_nullable_cstr = SPI_getvalue(tuple, tupdesc, 4);

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
                char *col_default_cstr = SPI_getvalue(tuple, tupdesc, 5);
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
    PG_FUNCTION_INFO_V1(explain_query);
    PG_FUNCTION_INFO_V1(explain_error);

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
            "      Supports SELECT, DDL (CREATE/ALTER/DROP), and DML (INSERT/UPDATE/DELETE)\n"
            "      ⚠️  DDL/DML queries are generated with disclaimers and NOT executed\n"
            "      Example: SELECT ai_toolkit.query('show active users');\n"
            "      Example: SELECT ai_toolkit.query('create a users table');\n\n"
            "  • ai_toolkit.explain_query([text])  \n"
            "      Get AI-powered explanation of a SQL query (returns void, shows via NOTICE)\n"
            "      If no query provided, explains the last executed query in session\n"
            "      🔄 Auto-tracks ALL queries from any source (CLI, apps, tools)\n"
            "      Example: SELECT ai_toolkit.explain_query('SELECT * FROM users');\n"
            "      Example: SELECT * FROM orders; -- then: SELECT ai_toolkit.explain_query();\n\n"
            "  • ai_toolkit.explain_error([text])  \n"
            "      Get AI-powered explanation and solution for an error (returns void, shows via NOTICE)\n"
            "      If no error provided, explains the last error in session\n"
            "      🔄 Auto-tracks ALL errors from any source\n"
            "      Example: SELECT ai_toolkit.explain_error('syntax error at...');\n"
            "      Example: After any error, call: SELECT ai_toolkit.explain_error();\n\n"
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
            "⚠️  IMPORTANT: DDL and DML queries are generated for reference only.\n"
            "    They are displayed with a disclaimer and NOT executed automatically.\n"
            "    Always review such queries carefully before manual execution.\n\n"
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
            std::string system_prompt = load_system_prompt();

            user_prompt = "User request: `" + user_prompt + "`\n"
                                                            "Generate a valid Postgres query based on the request. "
                                                            "Follow the strict step-by-step process in the system prompt. "
                                                            "Use the available tools to explore the database schema and retrieve necessary information. "
                                                            "Only 10 Tools Calls are available use them very wisely, if you really don't have information then only call, do not spam it."
                                                            "If the query involves DDL (CREATE, ALTER, DROP) or DML (INSERT, UPDATE, DELETE), "
                                                            "you MUST include a <disclaimer> tag at the beginning of your response with a warning message, "
                                                            "followed by the SQL query in <sql> tags. The query will NOT be executed, only shown to the user.";

            // Configure generation options with tools
            ai::GenerateOptions options(model, system_prompt, user_prompt);
            options.tools["set_memory"] = set_memory_tool;
            options.tools["get_memory"] = get_memory_tool;
            options.tools["list_schemas"] = list_schemas_tool;
            options.tools["list_tables_in_schema"] = list_tables_tool;
            options.tools["get_schema_for_table"] = get_schema_tool;
            options.max_steps = 10; // Allow multi-step reasoning with tool calls

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
                // Parse SQL query and disclaimer from response
                std::string response_text = result.text;
                std::string sql_query;
                std::string disclaimer;
                bool has_disclaimer = false;

                // Look for <disclaimer> ... </disclaimer> pattern
                size_t disclaimer_start = response_text.find("<disclaimer>");
                if (disclaimer_start != std::string::npos)
                {
                    disclaimer_start += 12; // Move past "<disclaimer>"
                    size_t disclaimer_end = response_text.find("</disclaimer>", disclaimer_start);
                    if (disclaimer_end != std::string::npos)
                    {
                        disclaimer = response_text.substr(disclaimer_start, disclaimer_end - disclaimer_start);
                        // Trim whitespace
                        size_t first = disclaimer.find_first_not_of(" \n\r\t");
                        size_t last = disclaimer.find_last_not_of(" \n\r\t");
                        if (first != std::string::npos && last != std::string::npos)
                        {
                            disclaimer = disclaimer.substr(first, last - first + 1);
                        }
                        has_disclaimer = true;
                    }
                }

                // Look for <sql> ... </sql> pattern
                size_t sql_start = response_text.find("<sql>");
                if (sql_start != std::string::npos)
                {
                    sql_start += 5; // Move past "<sql>"
                    size_t sql_end = response_text.find("</sql>", sql_start);
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
                    // Store the query in session memory for explain_query function
                    memory_set_core("session", "last_query", sql_query, "Last executed query in session", nullptr, false);

                    // Check if query is DDL or DML by examining the first keyword
                    std::string query_upper = sql_query;
                    std::transform(query_upper.begin(), query_upper.end(), query_upper.begin(), ::toupper);

                    // Remove leading whitespace and comments
                    size_t first_non_space = query_upper.find_first_not_of(" \n\r\t");
                    if (first_non_space != std::string::npos)
                    {
                        query_upper = query_upper.substr(first_non_space);
                    }

                    // List of DDL and DML keywords
                    bool is_ddl_dml = false;
                    std::vector<std::string> ddl_dml_keywords = {
                        "CREATE", "ALTER", "DROP", "TRUNCATE", "RENAME",
                        "INSERT", "UPDATE", "DELETE", "MERGE", "REPLACE",
                        "GRANT", "REVOKE"};

                    for (const auto &keyword : ddl_dml_keywords)
                    {
                        if (query_upper.find(keyword) == 0 ||
                            query_upper.find(keyword + " ") == 0 ||
                            query_upper.find(keyword + "\n") == 0 ||
                            query_upper.find(keyword + "\t") == 0)
                        {
                            is_ddl_dml = true;
                            break;
                        }
                    }

                    // If it's DDL/DML (either has disclaimer or detected by keywords), don't execute
                    if (has_disclaimer || is_ddl_dml)
                    {
                        std::stringstream output;
                        output << "\n⚠️  DISCLAIMER ⚠️\n";
                        output << "═══════════════════════════════════════════════════════════\n";

                        if (has_disclaimer && !disclaimer.empty())
                        {
                            output << disclaimer << "\n";
                        }
                        else
                        {
                            output << "This query involves data modification or schema changes.\n";
                            output << "It is generated for reference only and should not be executed\n";
                            output << "without proper review and backups.\n";
                        }

                        output << "═══════════════════════════════════════════════════════════\n\n";
                        output << "📋 Generated Query (NOT EXECUTED):\n";
                        output << "───────────────────────────────────────────────────────────\n";
                        output << sql_query << "\n";
                        output << "───────────────────────────────────────────────────────────\n";
                        output << "\nℹ️  This query was generated for reference only and has NOT been executed.\n";
                        output << "   Please review carefully before running it manually.\n";

                        elog(NOTICE, "%s", output.str().c_str());
                        SPI_finish();
                        PG_RETURN_VOID();
                    }

                    // Execute the SQL query (only for SELECT and other safe queries)
                    elog(NOTICE, "\n📋 Generated Query:\n%s\n", sql_query.c_str()); // SPI is already connected from the beginning of the function
                    int ret = SPI_execute(sql_query.c_str(), true, 0);

                    if (ret < 0)
                    {
                        std::string error_info = "Query execution failed with SPI error code: " + std::to_string(ret);
                        memory_set_core("session", "last_error", error_info, "Last error in session", nullptr, false);

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
                             errmsg("No SQL query found in response. Expected format: <sql><query></sql>")));
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

    /**
     * Explain Query function - AI-powered explanation of SQL queries
     * Takes optional query text, or uses last executed query from session
     */
    Datum explain_query(PG_FUNCTION_ARGS)
    {
        try
        {
            if (!openrouter_api_key)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("ai_toolkit.openrouter_api_key not set")));
            }

            // Query parameter is now required
            if (PG_ARGISNULL(0))
            {
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("Query parameter is required. Usage: SELECT ai_toolkit.explain_query('your query here');")));
            }

            // Get the query from the parameter
            text *query_text = PG_GETARG_TEXT_PP(0);
            std::string query_to_explain = std::string(VARDATA_ANY(query_text), VARSIZE_ANY_EXHDR(query_text));

            if (SPI_connect() != SPI_OK_CONNECT)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                         errmsg("Failed to connect to SPI")));
            }

            std::string api_key(openrouter_api_key);
            std::string base_url = openrouter_base_url ? openrouter_base_url : "https://openrouter.ai/api";
            std::string model = openrouter_model ? std::string(openrouter_model) : "meta-llama/llama-3.2-3b-instruct:free";

            // Create AI client
            auto client = ai::openai::create_client(api_key, base_url);

            // Define tools
            ai::Tool get_memory_tool = ai::create_simple_tool(
                "get_memory",
                "Retrieve previously stored information about database schema, tables, columns, relationships, or business rules. "
                "Parameters: category (table|column|relationship|business_rule|data_pattern|calculation|permission|custom), "
                "key (identifier like table name or 'table.column')",
                {{"category", "string"}, {"key", "string"}},
                tool_get_memory);

            ai::Tool list_schemas_tool = ai::create_simple_tool(
                "list_schemas",
                "List all available schemas in the current PostgreSQL database. No parameters required.",
                {},
                tool_list_schemas);

            ai::Tool list_tables_tool = ai::create_simple_tool(
                "list_tables_in_schema",
                "List all tables in a specific schema. Parameters: schema (name of the schema)",
                {{"schema", "string"}},
                tool_list_tables_in_schema);

            ai::Tool get_schema_tool = ai::create_simple_tool(
                "get_schema_for_table",
                "Get the CREATE TABLE statement (schema) for a specific table. "
                "Parameters: table_name (name of table, optionally prefixed with schema like 'schema.table')",
                {{"table_name", "string"}},
                tool_get_schema_for_table);

            // Build explanation prompt
            std::string system_prompt =
                "You are a PostgreSQL database expert. Your role is to explain SQL queries in detail.\n\n"
                "When explaining a query:\n"
                "1. Use available tools to understand the database schema\n"
                "2. Break down the query into logical components\n"
                "3. Explain what each part does\n"
                "4. Identify potential issues or optimization opportunities\n"
                "5. Use get_memory to check for stored context about tables/columns\n\n"
                "Provide your explanation in clear, structured format with:\n"
                "- Query purpose/goal\n"
                "- Step-by-step breakdown\n"
                "- Performance considerations\n"
                "- Any recommendations\n";

            std::string user_prompt = "Explain this SQL query in detail:\n\n" + query_to_explain;

            // Configure generation options
            ai::GenerateOptions options(model, system_prompt, user_prompt);
            options.tools["get_memory"] = get_memory_tool;
            options.tools["list_schemas"] = list_schemas_tool;
            options.tools["list_tables_in_schema"] = list_tables_tool;
            options.tools["get_schema_for_table"] = get_schema_tool;
            options.max_steps = 8;

            // Generate explanation
            auto result = client.generate_text(options);

            SPI_finish();

            if (result)
            {
                std::string explanation = "\n📖 Query Explanation\n"
                                          "═══════════════════════════════════════════════════════════\n"
                                          "Query:\n" +
                                          query_to_explain + "\n\n"
                                                             "Explanation:\n" +
                                          result.text + "\n"
                                                        "═══════════════════════════════════════════════════════════\n";

                elog(NOTICE, "%s", explanation.c_str());
                PG_RETURN_VOID();
            }
            else
            {
                std::string error_msg = "Failed to generate explanation: " + result.error_message();
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
                     errmsg("Exception in explain_query: %s", e.what())));
        }
    }

    /**
     * Explain Error function - AI-powered explanation of SQL errors
     * Takes optional error text, or uses last error from session
     */
    Datum explain_error(PG_FUNCTION_ARGS)
    {
        try
        {
            if (!openrouter_api_key)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("ai_toolkit.openrouter_api_key not set")));
            }

            // Error parameter is now required
            if (PG_ARGISNULL(0))
            {
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("Error message parameter is required. Usage: SELECT ai_toolkit.explain_error('your error message here');")));
            }

            // Get the error from the parameter
            text *error_text = PG_GETARG_TEXT_PP(0);
            std::string error_to_explain = std::string(VARDATA_ANY(error_text), VARSIZE_ANY_EXHDR(error_text));

            if (SPI_connect() != SPI_OK_CONNECT)
            {
                ereport(ERROR,
                        (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                         errmsg("Failed to connect to SPI")));
            }

            std::string api_key(openrouter_api_key);
            std::string base_url = openrouter_base_url ? openrouter_base_url : "https://openrouter.ai/api";
            std::string model = openrouter_model ? std::string(openrouter_model) : "meta-llama/llama-3.2-3b-instruct:free";

            // Create AI client
            auto client = ai::openai::create_client(api_key, base_url);

            // Define tools
            ai::Tool get_memory_tool = ai::create_simple_tool(
                "get_memory",
                "Retrieve previously stored information about database schema, tables, columns, relationships, or business rules. "
                "Parameters: category (table|column|relationship|business_rule|data_pattern|calculation|permission|custom), "
                "key (identifier like table name or 'table.column')",
                {{"category", "string"}, {"key", "string"}},
                tool_get_memory);

            ai::Tool list_schemas_tool = ai::create_simple_tool(
                "list_schemas",
                "List all available schemas in the current PostgreSQL database. No parameters required.",
                {},
                tool_list_schemas);

            ai::Tool list_tables_tool = ai::create_simple_tool(
                "list_tables_in_schema",
                "List all tables in a specific schema. Parameters: schema (name of the schema)",
                {{"schema", "string"}},
                tool_list_tables_in_schema);

            ai::Tool get_schema_tool = ai::create_simple_tool(
                "get_schema_for_table",
                "Get the CREATE TABLE statement (schema) for a specific table. "
                "Parameters: table_name (name of table, optionally prefixed with schema like 'schema.table')",
                {{"table_name", "string"}},
                tool_get_schema_for_table);

            // Build explanation prompt
            std::string system_prompt =
                "You are a PostgreSQL database expert specializing in debugging and error resolution.\n\n"
                "When explaining an error:\n"
                "1. Identify the error type and root cause\n"
                "2. Use available tools to understand the database context if needed\n"
                "3. Explain what went wrong in simple terms\n"
                "4. Provide step-by-step solutions\n"
                "5. Suggest best practices to avoid similar errors\n\n"
                "Provide your explanation in clear, structured format with:\n"
                "- Error type and cause\n"
                "- Why it happened\n"
                "- How to fix it (with examples if applicable)\n"
                "- Prevention tips\n";

            std::string user_prompt = "Explain this PostgreSQL error and provide solutions:\n\n" + error_to_explain;

            // Configure generation options
            ai::GenerateOptions options(model, system_prompt, user_prompt);
            options.tools["get_memory"] = get_memory_tool;
            options.tools["list_schemas"] = list_schemas_tool;
            options.tools["list_tables_in_schema"] = list_tables_tool;
            options.tools["get_schema_for_table"] = get_schema_tool;
            options.max_steps = 8;

            // Generate explanation
            auto result = client.generate_text(options);

            SPI_finish();

            if (result)
            {
                std::string explanation = "\n🔧 Error Explanation\n"
                                          "═══════════════════════════════════════════════════════════\n"
                                          "Error:\n" +
                                          error_to_explain + "\n\n"
                                                             "Analysis & Solution:\n" +
                                          result.text + "\n"
                                                        "═══════════════════════════════════════════════════════════\n";

                elog(NOTICE, "%s", explanation.c_str());
                PG_RETURN_VOID();
            }
            else
            {
                std::string error_msg = "Failed to generate explanation: " + result.error_message();
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
                     errmsg("Exception in explain_error: %s", e.what())));
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

        DefineCustomStringVariable("ai_toolkit.prompt_file",
                                   "AI Prompt File Path",
                                   "Path to a text file containing the system prompt for the AI. "
                                   "If not set or file does not exist, uses the default hardcoded prompt. "
                                   "Allows changing the prompt without rebuilding the extension.",
                                   &prompt_file_path,
                                   nullptr,
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

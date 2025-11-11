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
     */
    bool memory_set_core(const std::string &category, const std::string &key,
                         const std::string &value, const std::optional<std::string> &notes,
                         std::string *error_msg = nullptr)
    {
        std::string sql = "INSERT INTO ai_toolkit.ai_memory (category, key, value, notes, updated_at) "
                          "VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP) "
                          "ON CONFLICT (category, key) DO UPDATE SET "
                          "value = EXCLUDED.value, notes = EXCLUDED.notes, updated_at = CURRENT_TIMESTAMP";

        if (SPI_connect() != SPI_OK_CONNECT)
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
     */
    std::optional<std::string> memory_get_core(const std::string &category, const std::string &key,
                                               std::string *error_msg = nullptr)
    {
        std::string sql = "SELECT value FROM ai_toolkit.ai_memory WHERE category = $1 AND key = $2";

        if (SPI_connect() != SPI_OK_CONNECT)
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
                SPI_finish();
                return result;
            }
        }

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
            bool success = memory_set_core(category, key, value, notes, &error_msg);

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
            auto result = memory_get_core(category, key, &error_msg);

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
     * Helper function to get database schema information
     */
    std::string get_schema_context()
    {
        std::stringstream schema_info;

        if (SPI_connect() != SPI_OK_CONNECT)
        {
            return "Error: Could not retrieve schema information";
        }

        // Query to get all tables in the current database (excluding system schemas)
        const char *tables_sql =
            "SELECT schemaname, tablename, "
            "       obj_description((schemaname||'.'||tablename)::regclass, 'pg_class') as description "
            "FROM pg_tables "
            "WHERE schemaname NOT IN ('pg_catalog', 'information_schema', 'pg_toast') "
            "ORDER BY schemaname, tablename";

        int ret = SPI_execute(tables_sql, true, 0);

        if (ret == SPI_OK_SELECT && SPI_processed > 0)
        {
            schema_info << "=== DATABASE SCHEMA ===\n\n";

            for (uint64 i = 0; i < SPI_processed; i++)
            {
                bool isnull;
                HeapTuple tuple = SPI_tuptable->vals[i];
                TupleDesc tupdesc = SPI_tuptable->tupdesc;

                Datum schema_datum = SPI_getbinval(tuple, tupdesc, 1, &isnull);
                Datum table_datum = SPI_getbinval(tuple, tupdesc, 2, &isnull);
                Datum desc_datum = SPI_getbinval(tuple, tupdesc, 3, &isnull);

                std::string schema_name = TextDatumGetCString(schema_datum);
                std::string table_name = TextDatumGetCString(table_datum);
                std::string description = isnull ? "" : TextDatumGetCString(desc_datum);

                schema_info << "Table: " << schema_name << "." << table_name;
                if (!description.empty())
                {
                    schema_info << " - " << description;
                }
                schema_info << "\n";

                // Get columns for this table
                std::string columns_sql =
                    "SELECT column_name, data_type, is_nullable, column_default "
                    "FROM information_schema.columns "
                    "WHERE table_schema = '" +
                    schema_name + "' AND table_name = '" + table_name + "' "
                                                                        "ORDER BY ordinal_position";

                int col_ret = SPI_execute(columns_sql.c_str(), true, 0);

                if (col_ret == SPI_OK_SELECT && SPI_processed > 0)
                {
                    for (uint64 j = 0; j < SPI_processed; j++)
                    {
                        HeapTuple col_tuple = SPI_tuptable->vals[j];
                        TupleDesc col_tupdesc = SPI_tuptable->tupdesc;

                        Datum col_name_datum = SPI_getbinval(col_tuple, col_tupdesc, 1, &isnull);
                        Datum col_type_datum = SPI_getbinval(col_tuple, col_tupdesc, 2, &isnull);
                        Datum col_nullable_datum = SPI_getbinval(col_tuple, col_tupdesc, 3, &isnull);

                        std::string col_name = TextDatumGetCString(col_name_datum);
                        std::string col_type = TextDatumGetCString(col_type_datum);
                        std::string col_nullable = TextDatumGetCString(col_nullable_datum);

                        schema_info << "  - " << col_name << " (" << col_type << ")";
                        if (col_nullable == "NO")
                        {
                            schema_info << " NOT NULL";
                        }
                        schema_info << "\n";
                    }
                }

                schema_info << "\n";
            }
        }
        else
        {
            schema_info << "No user tables found in database.\n";
        }

        SPI_finish();
        return schema_info.str();
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

            // Get database schema information
            std::string schema_context = get_schema_context();

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

            // Build system prompt with schema context
            std::string system_prompt =
                "You are a PostgreSQL database assistant. Your role is to help users write SELECT queries.\n\n"
                "=== AVAILABLE DATABASE SCHEMA ===\n" +
                schema_context + "\n"
                                 "=== STRICT QUERY RESTRICTIONS ===\n"
                                 "- ONLY SELECT queries are allowed\n"
                                 "- NEVER generate DROP, DELETE, UPDATE, or INSERT queries\n"
                                 "- If user requests data modification operations, respond: 'I can only execute SELECT queries. Data modification operations are not permitted.'\n\n"
                                 "=== OPERATIONAL GUIDELINES ===\n"
                                 "1. Check available context using get_memory tool for relevant tables/columns\n"
                                 "2. If you have sufficient context from schema and memory to build a reasonable query, build it immediately - DO NOT ask questions\n"
                                 "3. Only deny the request if the query is truly impossible without additional information\n"
                                 "4. Make reasonable assumptions based on standard SQL conventions and available schema\n"
                                 "5. Use common sense for relationships (e.g., foreign key patterns, id matching)\n"
                                 "6. Use set_memory tool to save important context you learn for future queries\n\n"
                                 "=== MEMORY TOOL USAGE ===\n"
                                 "- Use get_memory('table', 'table_name') to get table descriptions\n"
                                 "- Use get_memory('column', 'table.column') to get column details\n"
                                 "- Use get_memory('relationship', 'table1_table2') to understand joins\n"
                                 "- Use get_memory('business_rule', 'rule_name') for business logic\n"
                                 "- Use set_memory when you discover patterns or relationships\n\n"
                                 "=== RESPONSE FORMAT ===\n"
                                 "If you can build the query:\n"
                                 "1. SQL Query: [The SELECT query]\n"
                                 "2. Explanation: [Brief explanation of what the query does]\n\n"
                                 "If you cannot build the query:\n"
                                 "1. Response: 'I cannot generate this query because [specific reason]. Please provide [specific information needed].'\n\n"
                                 "Available memory categories: table, column, relationship, business_rule, data_pattern, "
                                 "calculation, permission, custom";

            // Configure generation options with tools
            ai::GenerateOptions options(model, system_prompt, user_prompt);
            options.tools["set_memory"] = set_memory_tool;
            options.tools["get_memory"] = get_memory_tool;
            options.max_steps = 5; // Allow multi-step reasoning with tool calls

            // Generate response
            auto result = client.generate_text(options);

            if (result)
            {
                // Format response with tool usage information
                std::stringstream response;
                response << result.text;

                if (result.has_tool_calls())
                {
                    response << "\n\n[Tool Calls Made: " << result.tool_calls.size() << "]";
                }

                PG_RETURN_TEXT_P(cstring_to_text(response.str().c_str()));
            }
            else
            {
                std::string error_msg = "AI query failed: " + result.error_message();
                if (result.error.has_value())
                {
                    error_msg += " | " + result.error.value();
                }

                ereport(ERROR,
                        (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                         errmsg("%s", error_msg.c_str())));
            }
        }
        catch (const std::exception &e)
        {
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

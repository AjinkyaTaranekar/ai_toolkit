#include <cstdlib>
#include <iostream>
#include <string>

#include <ai/ai.h>
#include <ai/logger.h>
#include <ai/openai.h>

extern "C"
{
#include <postgres.h>
#include <fmgr.h>
#include <utils/numeric.h>
#include <utils/builtins.h>
#include "utils/guc.h"

#ifdef PG_MODULE_MAGIC
    PG_MODULE_MAGIC;
#endif

    // Global configuration variables
    static char *openrouter_api_key = nullptr;
    static char *openrouter_model = nullptr;
    static char *openrouter_base_url = nullptr;

    PG_FUNCTION_INFO_V1(factorial);
    PG_FUNCTION_INFO_V1(hello);

    Datum factorial(PG_FUNCTION_ARGS)
    {
        int32 arg = PG_GETARG_INT32(0);

        if (arg < 0)
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("Factorial input must be a non-negative integer")));

        int64 result = 1;
        for (int i = 2; i <= arg; ++i)
            result *= i;

        PG_RETURN_INT64(result);
    }

    Datum hello(PG_FUNCTION_ARGS)
    {
        text *name = PG_GETARG_TEXT_PP(0);
        char *name_str = text_to_cstring(name);

        try
        {
            if (!openrouter_api_key)
            {
                pfree(name_str);
                ereport(ERROR,
                        (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                         errmsg("ai_toolkit.openrouter_api_key not set")));
            }
            std::string api_key(openrouter_api_key);

            // Create OpenAI client with OpenRouter's base URL
            auto client = ai::openai::create_client(api_key, openrouter_base_url ? openrouter_base_url : "https://openrouter.ai/api/v1");

            // Create a personalized greeting prompt
            std::string prompt = "Generate a warm, friendly greeting for someone named " +
                                 std::string(name_str) +
                                 ". Keep it brief and welcoming (1-2 sentences).";

            // Generate greeting using OpenRouter
            ai::GenerateOptions options(
                openrouter_model ? std::string(openrouter_model) : "meta-llama/llama-3.2-3b-instruct:free",
                "You are a friendly assistant that creates personalized greetings.",
                prompt);

            auto result = client.generate_text(options);

            pfree(name_str);

            if (result)
            {
                // Return the AI-generated greeting
                PG_RETURN_TEXT_P(cstring_to_text(result.text.c_str()));
            }
            else
            {
                ereport(ERROR,
                        (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                         errmsg("OpenRouter API error: %s", result.error_message().c_str())));
            }
        }
        catch (const std::exception &e)
        {
            pfree(name_str);
            ereport(ERROR,
                    (errcode(ERRCODE_EXTERNAL_ROUTINE_EXCEPTION),
                     errmsg("Exception in hello function: %s", e.what())));
        }
    }
}

// Module initialization
extern "C"
{
    void _PG_init(void)
    {
        // Define custom GUC variables
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
                                   "https://openrouter.ai/api/v1",
                                   PGC_USERSET,
                                   0,
                                   nullptr,
                                   nullptr,
                                   nullptr);

        ereport(LOG,
                (errmsg("ai_toolkit extension loaded")));
    }

    void _PG_fini(void)
    {
        ereport(LOG,
                (errmsg("ai_toolkit extension unloaded")));
    }
}
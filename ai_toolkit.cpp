extern "C" {
    #include <postgres.h>
    #include <fmgr.h>
    #include <utils/numeric.h>


    #ifdef PG_MODULE_MAGIC
    PG_MODULE_MAGIC;
    #endif

    PG_FUNCTION_INFO_V1(factorial);

    Datum factorial(PG_FUNCTION_ARGS) {
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
}
-- pg_ai_toolkit--1.0.sql

-- Create the extension schema
CREATE SCHEMA pg_ai_toolkit;

-- Create the SQL function
CREATE OR REPLACE FUNCTION pg_ai_toolkit.factorial(integer)
RETURNS bigint AS 'pg_ai_toolkit', 'factorial'
LANGUAGE C STRICT;

-- Grant execute permission to public (change to appropriate roles if needed)
GRANT EXECUTE ON FUNCTION pg_ai_toolkit.factorial(integer) TO PUBLIC;
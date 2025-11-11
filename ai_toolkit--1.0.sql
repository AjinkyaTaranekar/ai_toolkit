-- ai_toolkit--1.0.sql

-- Create the extension schema
CREATE SCHEMA ai_toolkit;

-- Create the SQL function
CREATE OR REPLACE FUNCTION ai_toolkit.factorial(integer)
RETURNS bigint AS 'ai_toolkit', 'factorial'
LANGUAGE C STRICT;

-- Grant execute permission to public (change to appropriate roles if needed)
GRANT EXECUTE ON FUNCTION ai_toolkit.factorial(integer) TO PUBLIC;
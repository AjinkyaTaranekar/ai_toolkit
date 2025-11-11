-- ai_toolkit--1.0.sql

-- Create the extension schema
CREATE SCHEMA IF NOT EXISTS ai_toolkit;

-- ==========================================
-- Memory and Logging Tables
-- ==========================================

-- Memory table: stores knowledge about tables, columns, relationships, business rules
CREATE TABLE ai_toolkit.ai_memory (
    id SERIAL PRIMARY KEY,
    category TEXT NOT NULL,
    key TEXT NOT NULL,
    value TEXT NOT NULL,
    notes TEXT,
    confidence_score INTEGER DEFAULT 100,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by TEXT DEFAULT CURRENT_USER,
    UNIQUE(category, key)
);

CREATE INDEX idx_ai_memory_category_key ON ai_toolkit.ai_memory(category, key);

-- Query log: audit trail of all AI operations
CREATE TABLE ai_toolkit.ai_query_log (
    id SERIAL PRIMARY KEY,
    session_id TEXT,
    function_name TEXT NOT NULL,
    user_prompt TEXT,
    ai_analysis TEXT,
    sql_generated TEXT,
    user_approved BOOLEAN,
    execution_result TEXT,
    execution_time_ms INTEGER,
    tokens_used INTEGER,
    status TEXT,
    error_message TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    created_by TEXT DEFAULT CURRENT_USER
);

CREATE INDEX idx_ai_query_log_created_at ON ai_toolkit.ai_query_log(created_at);

-- ==========================================
-- Core C Functions
-- ==========================================

-- Help function - provides toolkit documentation
CREATE OR REPLACE FUNCTION ai_toolkit.help()
RETURNS text AS 'ai_toolkit', 'help'
LANGUAGE C STRICT;

-- Set memory function - stores context about database
CREATE OR REPLACE FUNCTION ai_toolkit.set_memory(text, text, text, text DEFAULT NULL)
RETURNS text AS 'ai_toolkit', 'set_memory'
LANGUAGE C STRICT;

-- Get memory function - retrieves stored context
CREATE OR REPLACE FUNCTION ai_toolkit.get_memory(text, text)
RETURNS text AS 'ai_toolkit', 'get_memory'
LANGUAGE C STRICT;

-- Query function - main AI-powered natural language query generator
CREATE OR REPLACE FUNCTION ai_toolkit.query(text)
RETURNS void AS 'ai_toolkit', 'query'
LANGUAGE C STRICT;

-- ==========================================
-- Helper SQL Functions
-- ==========================================

-- View all memories
CREATE OR REPLACE FUNCTION ai_toolkit.view_memories()
RETURNS TABLE(category TEXT, key TEXT, value TEXT, notes TEXT, updated_at TIMESTAMP) AS $$
BEGIN
    RETURN QUERY SELECT m.category, m.key, m.value, m.notes, m.updated_at 
    FROM ai_toolkit.ai_memory m 
    ORDER BY m.updated_at DESC;
END;
$$ LANGUAGE plpgsql;

-- Search memories
CREATE OR REPLACE FUNCTION ai_toolkit.search_memory(search_term TEXT)
RETURNS TABLE(category TEXT, key TEXT, value TEXT, notes TEXT) AS $$
BEGIN
    RETURN QUERY SELECT m.category, m.key, m.value, m.notes
    FROM ai_toolkit.ai_memory m 
    WHERE m.value ILIKE '%' || search_term || '%' 
       OR m.notes ILIKE '%' || search_term || '%'
       OR m.key ILIKE '%' || search_term || '%'
    ORDER BY m.updated_at DESC;
END;
$$ LANGUAGE plpgsql;

-- View recent query logs
CREATE OR REPLACE FUNCTION ai_toolkit.view_logs(limit_rows INTEGER DEFAULT 50)
RETURNS TABLE(
    id INTEGER,
    function_name TEXT,
    user_prompt TEXT,
    sql_generated TEXT,
    status TEXT,
    created_at TIMESTAMP
) AS $$
BEGIN
    RETURN QUERY SELECT l.id, l.function_name, l.user_prompt, l.sql_generated, l.status, l.created_at
    FROM ai_toolkit.ai_query_log l 
    ORDER BY l.created_at DESC 
    LIMIT limit_rows;
END;
$$ LANGUAGE plpgsql;

-- ==========================================
-- Permissions
-- ==========================================

GRANT SELECT, INSERT, UPDATE ON ai_toolkit.ai_memory TO PUBLIC;
GRANT SELECT, INSERT ON ai_toolkit.ai_query_log TO PUBLIC;
GRANT USAGE ON SEQUENCE ai_toolkit.ai_memory_id_seq TO PUBLIC;
GRANT USAGE ON SEQUENCE ai_toolkit.ai_query_log_id_seq TO PUBLIC;

GRANT EXECUTE ON FUNCTION ai_toolkit.help() TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.set_memory(text, text, text, text) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.get_memory(text, text) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.query(text) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.view_memories() TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.search_memory(text) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.view_logs(integer) TO PUBLIC;
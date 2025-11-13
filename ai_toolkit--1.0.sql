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

-- Explain query function - AI-powered explanation of SQL queries
CREATE OR REPLACE FUNCTION ai_toolkit.explain_query(text DEFAULT NULL)
RETURNS void AS 'ai_toolkit', 'explain_query'
LANGUAGE C;

-- Explain error function - AI-powered explanation of SQL errors
CREATE OR REPLACE FUNCTION ai_toolkit.explain_error(text DEFAULT NULL)
RETURNS void AS 'ai_toolkit', 'explain_error'
LANGUAGE C;

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

-- ==========================================
-- Permissions
-- ==========================================

GRANT SELECT, INSERT, UPDATE ON ai_toolkit.ai_memory TO PUBLIC;
GRANT USAGE ON SEQUENCE ai_toolkit.ai_memory_id_seq TO PUBLIC;

GRANT EXECUTE ON FUNCTION ai_toolkit.help() TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.set_memory(text, text, text, text) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.get_memory(text, text) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.query(text) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.explain_query(text) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.explain_error(text) TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.view_memories() TO PUBLIC;
GRANT EXECUTE ON FUNCTION ai_toolkit.search_memory(text) TO PUBLIC;
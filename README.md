# ai_toolkit

A PostgreSQL Extension that brings AI-powered Query Generation and Explanation Tools directly into your PostgreSQL database.


https://github.com/user-attachments/assets/3d81bf61-a5dc-4696-8633-54e56701ac39


## Features

- 💬 Natural Language to SQL - Generate queries from plain English
- 📖 Query Explanation - AI-powered SQL query explanations
- 🔧 Error Resolution - Get solutions for SQL errors
- 🧠 Contextual Memory - Store database context for better responses
- 🤖 Multi-Provider Support - OpenAI, Anthropic, OpenRouter

## Available Functions

Once installed, the extension provides the following functions:

### Core AI Functions

- **`ai_toolkit.query(text)`** - Generate and execute SQL queries from natural language

  ```sql
  SELECT ai_toolkit.query('Show me all customers who made purchases last month');
  ```

- **`ai_toolkit.explain_query(text)`** - Get AI-powered explanations of SQL queries

  ```sql
  SELECT ai_toolkit.explain_query('SELECT * FROM users WHERE created_at > NOW() - INTERVAL ''30 days''');
  ```

- **`ai_toolkit.explain_error(text)`** - Get helpful explanations and fixes for SQL errors

  ```sql
  SELECT ai_toolkit.explain_error('ERROR: column "user_name" does not exist');
  ```

### Memory Management Functions

Store and retrieve context about your database to improve AI responses:

- **`ai_toolkit.set_memory(category, key, value, notes)`** - Store database context

  ```sql
  SELECT ai_toolkit.set_memory('schema', 'users_table', 'Contains customer information', 'Primary key is user_id');
  SELECT ai_toolkit.set_memory('business', 'currency', 'USD', 'All prices in US dollars');
  ```

- **`ai_toolkit.get_memory(category, key)`** - Retrieve stored context

  ```sql
  SELECT ai_toolkit.get_memory('schema', 'users_table');
  ```

- **`ai_toolkit.view_memories()`** - View all stored memories

  ```sql
  SELECT * FROM ai_toolkit.view_memories();
  ```

- **`ai_toolkit.search_memory(search_term)`** - Search through stored memories

  ```sql
  SELECT * FROM ai_toolkit.search_memory('customer');
  ```

### Utility Functions

- **`ai_toolkit.help()`** - Display help and documentation

  ```sql
  SELECT ai_toolkit.help();
  ```

## Prerequisites

- PostgreSQL 18 or higher
- CMake 3.16 or higher
- C++ compiler with C++20 support
- Git
- PostgreSQL development headers (`postgresql-server-dev-18`)

## Installation Guide

### Step 1: Build the AI SDK Dependency

First, clone and build the AI SDK library:

```bash
# Clone the AI SDK with all dependencies
git clone --recursive https://github.com/ClickHouse/ai-sdk-cpp.git
cd ai-sdk-cpp

# Configure with position-independent code (required for PostgreSQL extensions)
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON \
  -DBUILD_EXAMPLES=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON

# Build the library
cmake --build build --parallel $(nproc)
```

### Step 2: Install the PostgreSQL Extension

Clone this repository into your PostgreSQL extensions directory:

```bash
cd /usr/share/postgresql/18/extension/
sudo git clone https://github.com/AjinkyaTaranekar/ai_toolkit.git ai_toolkit
cd ai_toolkit
```

Build and install the extension:

```bash
make clean && make && sudo make install
```

### Step 3: Configure PostgreSQL

Edit your PostgreSQL configuration file to add the required settings:

```bash
sudo nano /etc/postgresql/18/main/postgresql.conf
```

Add these lines at the end of the file (all fields are required):

```conf
# AI Toolkit Configuration (Required)
ai_toolkit.ai_provider = 'openai'                                        # Options: openai, anthropic, openrouter
ai_toolkit.ai_api_key = 'sk-YOUR-API-KEY-HERE'                          # Your API key
ai_toolkit.ai_model = 'gpt-4o'                                          # Model name
ai_toolkit.prompt_file = '/usr/share/postgresql/18/extension/ai_toolkit/prompts/query_system_prompt.txt'
```

**Examples by Provider:**

```conf
# OpenAI (GPT-4o)
ai_toolkit.ai_provider = 'openai'
ai_toolkit.ai_api_key = 'sk-YOUR-OPENAI-KEY'
ai_toolkit.ai_model = 'gpt-5'
ai_toolkit.prompt_file = '/usr/share/postgresql/18/extension/ai_toolkit/prompts/query_system_prompt.txt'

# Anthropic (Claude Sonnet 4.5)
ai_toolkit.ai_provider = 'anthropic'
ai_toolkit.ai_api_key = 'sk-ant-YOUR-ANTHROPIC-KEY'
ai_toolkit.ai_model = 'claude-sonnet-4-5'
ai_toolkit.prompt_file = '/usr/share/postgresql/18/extension/ai_toolkit/prompts/query_system_prompt.txt'

# OpenRouter (Free Models)
ai_toolkit.ai_provider = 'openrouter'
ai_toolkit.ai_api_key = 'sk-or-YOUR-OPENROUTER-KEY'
ai_toolkit.ai_model = 'google/gemini-2.0-flash-exp:free'
ai_toolkit.prompt_file = '/usr/share/postgresql/18/extension/ai_toolkit/prompts/query_system_prompt.txt'
```

**Note:** Replace the API key with your actual key. The `prompt_file` path should point to the system prompt file included with the extension.

### Step 4: Restart PostgreSQL

Restart PostgreSQL to load the new extension and configuration:

```bash
sudo systemctl restart postgresql
```

### Step 5: Enable the Extension in Your Database

Connect to PostgreSQL as the postgres user and create the extension:

```bash
sudo -u postgres psql
```

Then in the PostgreSQL prompt:

```sql
CREATE EXTENSION ai_toolkit;
```

### Step 6: (Optional) Load Sample Database

If you want to test with sample data:

```bash
sudo -u postgres psql -f sample_database.sql
```

### Verify Installation

Check that the extension is installed correctly:

```sql
SELECT * FROM pg_extension WHERE extname = 'ai_toolkit';
```

## Usage Examples

```sql
-- Generate SQL from natural language
SELECT ai_toolkit.query('show all users who signed up last week');

-- Explain a query
SELECT ai_toolkit.explain_query('SELECT * FROM users WHERE created_at > NOW() - INTERVAL ''7 days''');

-- Get help with errors
SELECT ai_toolkit.explain_error('ERROR: relation "user" does not exist');

-- Store context to improve AI responses
SELECT ai_toolkit.set_memory('table', 'users', 'Contains customer account information');
```

## Development Workflow

When making changes to the extension code:

1. Navigate to the extension directory:

   ```bash
   cd /usr/share/postgresql/18/extension/ai_toolkit
   ```

2. Pull latest changes (if working with a team):

   ```bash
   git pull
   ```

3. Rebuild and reinstall:

   ```bash
   make clean && make && sudo make install
   ```

4. Restart PostgreSQL:

   ```bash
   sudo systemctl restart postgresql
   ```

5. Test your changes:

   ```bash
   sudo -u postgres psql -d your_database
   ```

   Then in psql:

   ```sql
   DROP EXTENSION IF EXISTS ai_toolkit CASCADE;
   CREATE EXTENSION ai_toolkit;
   -- Test your changes here
   ```

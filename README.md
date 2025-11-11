# pg_ai_toolkit

A PostgreSQL Extension to bring AI-powered Query Generation and Explanation Tools right inside your PostgreSQL database.

## Prerequisites

- PostgreSQL 18 or higher installed
- CMake 3.16 or higher
- C++ compiler with C++20 support
- Git

## Building the Extension

### 1. Clone the Repository with Dependencies

```bash
git clone --recursive https://github.com/ClickHouse/ai-sdk-cpp.git
cd ai-sdk-cpp
```

### 2. Build the AI SDK

Configure and build the AI SDK with position-independent code:

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTS=ON \
  -DBUILD_EXAMPLES=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON

# Build
cmake --build build --parallel $(nproc)
```

### 3. Install the PostgreSQL Extension

1. Clone this repository to your PostgreSQL extensions directory (typically `/usr/share/postgresql/{VERSION}/extension/`):
   ```bash
   cd /usr/share/postgresql/18/extension/
   git clone https://github.com/AjinkyaTaranekar/pg_ai_toolkit.git ai_toolkit
   ```

2. Build and install the extension:
   ```bash
   cd ai_toolkit
   make
   sudo make install
   ```

3. Restart your PostgreSQL server to load the new extension:
   ```bash
   sudo systemctl restart postgresql
   ```

## Usage

### Enable the Extension

Connect to your database using `psql` and create the extension:

```sql
CREATE EXTENSION ai_toolkit;
```

### Verify Installation

Check that the extension is installed:

```sql
SELECT * FROM pg_extension WHERE extname = 'ai_toolkit';
```

## Development

To build and test the extension locally during development:

1. Ensure you have PostgreSQL development headers installed:
   ```bash
   # Ubuntu/Debian
   sudo apt-get install postgresql-server-dev-18
   ```

2. Make your changes to the source code

3. Rebuild and reinstall:
   ```bash
   make clean
   make
   sudo make install
   ```

4. Restart PostgreSQL and test your changes:
   ```bash
   sudo systemctl restart postgresql
   psql -d your_database -c "DROP EXTENSION IF EXISTS ai_toolkit; CREATE EXTENSION ai_toolkit;"
   ```


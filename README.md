# mod_replace - High-Performance Apache Module for Text Replacement

A high-performance Apache module for real-time text replacement in web content using the Aho-Corasick string matching algorithm. This module enables efficient multi-pattern search and replace operations with zero-copy optimizations and variable expansion support.

## Features

- **High Performance**: Aho-Corasick algorithm with O(n+m+z) complexity
- **Variable Expansion**: Support for `${VAR}` and `%{VAR}` environment variables
- **Zero-Copy Operations**: Optimized in-place replacements when possible
- **Smart Caching**: Precompiled automatons for maximum performance
- **Performance Monitoring**: Detailed timing logs for optimization
- **Content-Type Filtering**: Safe processing of text content only
- **Dynamic Configuration**: Real-time rule updates without server restart

## Quick Start

### Prerequisites

- Apache 2.4+
- APR development libraries
- CMake 3.10+
- GCC or compatible C compiler

### Installation

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install apache2-dev libapr1-dev cmake build-essential

# Clone and build
git clone <repository>
cd mod_replace
mkdir build && cd build
cmake ..
make

# Install module
sudo cp mod_replace.so /usr/lib/apache2/modules/
sudo a2enmod replace
sudo systemctl restart apache2
```

### Basic Configuration

Add to your Apache configuration or `.htaccess`:

```apache
# Enable text replacement
ReplaceEnable On

# Define replacement rules
ReplaceRule "old_text" "new_text"
ReplaceRule "search_pattern" "replacement_value"

# Apply to HTML files
<FilesMatch "\.(html|htm)$">
    SetOutputFilter REPLACE
</FilesMatch>
```

## Configuration Reference

### Directives

#### ReplaceEnable
**Syntax:** `ReplaceEnable On|Off`  
**Default:** `Off`  
**Context:** server config, virtual host, directory, .htaccess

Enables or disables text replacement processing.

```apache
ReplaceEnable On
```

#### ReplaceRule
**Syntax:** `ReplaceRule <search> <replacement>`  
**Context:** server config, virtual host, directory, .htaccess

Defines a search and replacement pattern. Multiple rules can be defined.

```apache
ReplaceRule "{{PLACEHOLDER}}" "Actual Content"
ReplaceRule "old_string" "new_string"
```

### Variable Expansion

The module supports environment variable expansion in replacement values:

```apache
# Using ${VAR} syntax
ReplaceRule "{{SERVER}}" "${SERVER_NAME}"

# Using %{VAR} syntax  
ReplaceRule "{{USER}}" "%{REMOTE_USER}"

# Environment variables
ReplaceRule "{{VERSION}}" "${APP_VERSION}"
```

### Advanced Configuration

#### Content Type Filtering

```apache
# Safe configuration with content type filtering
<FilesMatch "\.(html|htm)$">
    # Force correct content type
    ForceType text/html
    Header set Content-Type "text/html; charset=utf-8"
    
    # Enable replacement
    ReplaceEnable On
    SetOutputFilter REPLACE
</FilesMatch>

# Exclude binary files
<FilesMatch "\.(jpg|png|pdf|zip)$">
    RemoveOutputFilter REPLACE
</FilesMatch>
```

#### Performance Optimization

```apache
# Enable debug logging for performance monitoring
LogLevel replace:debug

# Directory-based configuration for better performance
<Directory "/var/www/html">
    ReplaceEnable On
    ReplaceRule "{{SITE_NAME}}" "My Website"
    
    <FilesMatch "\.(html|htm|php)$">
        SetOutputFilter REPLACE
    </FilesMatch>
</Directory>
```

## Performance

### Benchmark Results

The module uses an optimized Aho-Corasick implementation with significant performance improvements:

- **Fast Path (Precompiled)**: 10-50μs per request
- **Memory Efficient**: Shared automaton across requests  
- **Scalable**: O(n+m+z) complexity vs O(n×m×k) naive approach

### Performance Monitoring

Enable debug logging to monitor performance:

```apache
LogLevel replace:debug
```

Log output includes timing information:
```
mod_replace: Fast path completed - ac_time=15 μs, total_time=45 μs, input_len=1024, output_len=1056, patterns=3
mod_replace: Slow path completed - create_time=120 μs, compile_time=80 μs, replace_time=35 μs, total_time=235 μs
```

## Use Cases

### Template Variable Replacement

```apache
# Replace template variables in HTML
ReplaceEnable On
ReplaceRule "{{TITLE}}" "Welcome to My Site"
ReplaceRule "{{YEAR}}" "2024"
ReplaceRule "{{VERSION}}" "${APP_VERSION}"
```

### Dynamic Content Injection

```apache
# Inject dynamic content
ReplaceRule "{{USER_NAME}}" "%{REMOTE_USER}"
ReplaceRule "{{SERVER_INFO}}" "${SERVER_SOFTWARE}"
ReplaceRule "{{REQUEST_TIME}}" "%{REQUEST_TIME}"
```

### Asset URL Rewriting

```apache
# Rewrite asset URLs
ReplaceRule "/assets/" "/cdn/assets/v${ASSET_VERSION}/"
ReplaceRule "{{CDN_URL}}" "${CDN_BASE_URL}"
```

### API Endpoint Configuration

```apache
# Configure API endpoints
ReplaceRule "{{API_BASE}}" "${API_ENDPOINT}"
ReplaceRule "{{API_KEY}}" "%{API_SECRET}"
```

## Development

### Building from Source

```bash
# Standard build
mkdir build && cd build
cmake ..
make

# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run tests
make test

# Memory leak testing
make valgrind-test
```

### Project Structure

```
mod_replace/
├── src/
│   ├── mod_replace.c          # Main module implementation
│   └── aho_corasick.c         # Aho-Corasick algorithm
├── inc/
│   └── aho_corasick.h         # Algorithm header
├── test/
│   ├── test_aho_corasick.c    # Algorithm tests
│   └── test_standalone.c      # Standalone tests
└── CMakeLists.txt             # Build configuration
```

### Testing

The module includes comprehensive test suites:

```bash
# Run Aho-Corasick tests
./test_aho_corasick

# Run memory leak detection
make valgrind-aho-corasick

# Check build
make test
```

## Troubleshooting

### Common Issues

#### Module Not Loading
```bash
# Check if module is enabled
apache2ctl -M | grep replace_module

# Enable module
sudo a2enmod replace
sudo systemctl restart apache2
```

#### Rules Not Applied
```bash
# Check configuration syntax
apache2ctl configtest

# Verify output filter is set
grep -r "SetOutputFilter.*REPLACE" /etc/apache2/

# Check debug logs
sudo tail -f /var/log/apache2/error.log | grep mod_replace
```

#### Performance Issues
```bash
# Enable performance logging
LogLevel replace:debug

# Monitor timing
sudo tail -f /var/log/apache2/error.log | grep "total_time"
```

#### Encoding Problems
```bash
# Ensure UTF-8 encoding
Header set Content-Type "text/html; charset=utf-8"

# Force content type
ForceType text/html
```

### Debug Configuration

```apache
# Enable comprehensive logging
LogLevel replace:debug
LogLevel headers:debug

# Log file analysis
ErrorLog ${APACHE_LOG_DIR}/replace-debug.log

# Custom log format
LogFormat "%h %l %u %t \"%r\" %>s %O \"%{Referer}i\" \"%{User-Agent}i\" %D" combined_with_time
CustomLog ${APACHE_LOG_DIR}/replace-access.log combined_with_time
```

## Security Considerations

### Safe Configuration Practices

```apache
# Always filter content types
<FilesMatch "\.(html|htm)$">
    SetOutputFilter REPLACE
</FilesMatch>

# Exclude sensitive files
<FilesMatch "\.(conf|log|key)$">
    RemoveOutputFilter REPLACE
    Require all denied
</FilesMatch>

# Validate environment variables
<If "%{ENV:UNSAFE_VAR} != ''">
    ReplaceRule "{{SAFE_VAR}}" "default_value"
</If>
```

### Variable Security

- Never expose sensitive environment variables in replacement rules
- Validate all environment variable values
- Use appropriate access controls for configuration files
- Monitor logs for unexpected patterns

## License

This project is licensed under the Apache License 2.0. See LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request

### Development Guidelines

- Follow Apache module conventions
- Include comprehensive tests
- Update documentation
- Use memory-safe practices
- Profile performance impact

## Support

- **Issues**: Report bugs and feature requests via GitHub issues
- **Documentation**: See inline code documentation and examples
- **Performance**: Use debug logging for optimization guidance

## Changelog

### Version 1.0.0
- Initial release with Aho-Corasick implementation
- Variable expansion support
- Performance monitoring
- Content-type filtering
- Comprehensive test suite
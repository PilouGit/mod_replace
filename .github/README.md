# GitHub Actions CI/CD Pipeline

This directory contains the GitHub Actions workflows for automated building, testing, and deployment of the mod_replace Apache module.

## Workflows

### build.yml - Full Build and Release Pipeline
**Triggers:** Push to main/develop, Pull Requests, Releases

**Jobs:**
- **build-ubuntu**: Builds and tests on Ubuntu 20.04, 22.04, 24.04
- **build-rockylinux**: Builds and tests on Rocky Linux 9  
- **security-scan**: Runs CodeQL security analysis
- **create-release**: Creates release packages for GitHub releases

**Features:**
- Multi-platform builds (Ubuntu, Rocky Linux)
- Apache module installation testing
- Memory leak detection with Valgrind
- Automated release artifact creation
- Security scanning with CodeQL

### test.yml - Quick Test Pipeline  
**Triggers:** Push to any branch, Pull Requests

**Jobs:**
- **quick-test**: Fast build and unit tests
- **lint**: Code formatting and static analysis

**Features:**
- Rapid feedback for development
- Unit test execution
- Memory leak checking
- Code formatting validation
- Static analysis with cppcheck

## Build Matrix

### Ubuntu Builds
- Ubuntu 20.04 LTS (Apache 2.4)
- Ubuntu 22.04 LTS (Apache 2.4) 
- Ubuntu 24.04 LTS (Apache 2.4)

### Rocky Linux Builds
- Rocky Linux 9 (Apache 2.4)

## Test Coverage

### Unit Tests
- Aho-Corasick algorithm functionality
- Zero-copy operations
- Variable expansion
- Memory management

### Integration Tests  
- Apache module loading
- Configuration parsing
- Output filter processing
- Multi-pattern replacements

### Security Tests
- CodeQL static analysis
- Memory leak detection
- Buffer overflow protection
- Input validation

## Artifacts

### Build Artifacts
Generated for each successful build:
- `mod_replace.so` - Compiled Apache module
- `libaho_corasick.a` - Static library  
- `build-info.txt` - Build metadata

### Release Artifacts
Created automatically for GitHub releases:
- `mod_replace-ubuntu-22.04.tar.gz`
- `mod_replace-rockylinux-9.tar.gz`

## Development Workflow

### Branch Strategy
- **main**: Production-ready code
- **develop**: Integration branch for features
- **feature/***: Feature development branches

### CI Behavior
- **feature branches**: Quick tests only
- **develop branch**: Full build matrix
- **main branch**: Full build + security scan
- **releases**: Complete pipeline + artifact creation

### Quality Gates
All builds must pass:
1. ✅ Compilation on all platforms
2. ✅ Unit test execution  
3. ✅ Memory leak detection
4. ✅ Apache module loading
5. ✅ Security scan (main branch)

## Local Testing

To run the same tests locally:

```bash
# Quick tests
mkdir build && cd build
cmake ..
make -j$(nproc)
./test_aho_corasick
./test_valgrind
ctest

# Memory leak testing
make valgrind-aho-corasick

# Code formatting
find src inc test -name "*.c" -o -name "*.h" | \
  xargs clang-format --dry-run --Werror

# Static analysis  
cppcheck --enable=all src/ inc/
```

## Troubleshooting CI Issues

### Common Build Failures

**Dependencies missing:**
```yaml
- name: Install dependencies
  run: |
    sudo apt-get update
    sudo apt-get install -y apache2-dev libapr1-dev
```

**Test failures:**
- Check test output in GitHub Actions logs
- Run tests locally to reproduce
- Verify Apache module installation

**Memory leaks:**
- Review Valgrind log output
- Check for unfreed APR pools
- Validate automaton cleanup

### Debugging Tips

1. **Enable verbose logging:**
   ```bash
   make VERBOSE=1
   ```

2. **Check Apache configuration:**
   ```bash
   apache2ctl configtest
   apache2ctl -M | grep replace
   ```

3. **Manual module testing:**
   ```bash
   sudo cp mod_replace.so /usr/lib/apache2/modules/
   sudo a2enmod replace
   sudo systemctl restart apache2
   ```

## Security Considerations

### CodeQL Analysis
- Scans for security vulnerabilities
- Checks memory safety issues
- Validates input handling
- Reviews buffer operations

### Dependency Security
- Uses pinned action versions
- Validates package signatures
- Scans for known vulnerabilities
- Regular dependency updates

## Performance Monitoring

Build time optimization:
- Parallel compilation with `make -j$(nproc)`
- Cached dependencies where possible
- Minimal container images
- Efficient test execution

The CI pipeline ensures high-quality, secure, and performant builds across multiple platforms while providing rapid feedback to developers.
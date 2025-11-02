#!/usr/bin/env python3
"""
Generate test data for performance benchmarking
Creates patterns and HTML content for testing mod_replace vs mod_substitute
"""

import random
import string

def generate_patterns(count=100):
    """Generate simple replacement patterns"""
    patterns = []

    # Generate domain replacement patterns
    for i in range(count // 4):
        old_domain = f"api{i}.oldsite.com"
        new_domain = f"api{i}.newsite.com"
        patterns.append((old_domain, new_domain))

    # Generate URL path patterns
    for i in range(count // 4):
        old_path = f"/old/path{i}/"
        new_path = f"/new/path{i}/"
        patterns.append((old_path, new_path))

    # Generate common text replacements
    replacements = [
        ("CompanyOld", "CompanyNew"),
        ("OldProduct", "NewProduct"),
        ("deprecated_api", "current_api"),
        ("v1.0", "v2.0"),
        ("http://", "https://"),
        ("localhost:8080", "production.com"),
        ("test-env", "prod-env"),
        ("staging", "production"),
    ]
    for i in range(count // 4):
        patterns.append(replacements[i % len(replacements)])

    # Generate variable-like patterns
    for i in range(count // 4):
        old_var = f"OLD_VAR_{i}"
        new_var = f"NEW_VAR_{i}"
        patterns.append((old_var, new_var))

    # Fill remaining with random patterns
    while len(patterns) < count:
        idx = len(patterns)
        patterns.append((f"pattern{idx}", f"replacement{idx}"))

    return patterns[:count]

def generate_html_content(size_kb=100, pattern_density=0.1):
    """
    Generate HTML content with patterns embedded

    Args:
        size_kb: Approximate size of content in KB
        pattern_density: Fraction of content that contains patterns (0.0-1.0)
    """
    patterns = generate_patterns(100)

    content_parts = []
    content_parts.append("<!DOCTYPE html>\n<html>\n<head>\n")
    content_parts.append("  <title>Performance Test Page</title>\n")
    content_parts.append("  <meta charset='utf-8'>\n")
    content_parts.append("</head>\n<body>\n")

    # Generate content
    target_size = size_kb * 1024
    current_size = sum(len(p) for p in content_parts)

    paragraph_count = 0
    while current_size < target_size:
        # Add a paragraph
        content_parts.append(f"  <div class='section' id='section{paragraph_count}'>\n")
        content_parts.append(f"    <h2>Section {paragraph_count}</h2>\n")
        content_parts.append("    <p>")

        # Generate paragraph text with embedded patterns
        words = []
        for _ in range(50):  # 50 words per paragraph
            if random.random() < pattern_density:
                # Insert a pattern
                pattern, _ = random.choice(patterns)
                words.append(pattern)
            else:
                # Random word
                word_len = random.randint(3, 10)
                words.append(''.join(random.choices(string.ascii_lowercase, k=word_len)))

        content_parts.append(' '.join(words))
        content_parts.append("</p>\n")

        # Add some links with patterns
        if random.random() < 0.5:
            pattern, _ = random.choice(patterns[:25])  # Use domain patterns
            content_parts.append(f"    <a href='http://{pattern}/page'>Link to {pattern}</a>\n")

        content_parts.append("  </div>\n")

        paragraph_count += 1
        current_size = sum(len(p) for p in content_parts)

    content_parts.append("</body>\n</html>\n")

    return ''.join(content_parts)

def save_patterns_file(patterns, filename="patterns.txt"):
    """Save patterns in a simple format: search|replace"""
    with open(filename, 'w') as f:
        for search, replace in patterns:
            f.write(f"{search}|{replace}\n")
    print(f"Generated {len(patterns)} patterns in {filename}")

def save_html_file(content, filename="test_content.html"):
    """Save HTML content to file"""
    with open(filename, 'w') as f:
        f.write(content)
    size_kb = len(content) / 1024
    print(f"Generated {size_kb:.2f} KB of HTML content in {filename}")

if __name__ == "__main__":
    import sys

    # Parse arguments
    num_patterns = int(sys.argv[1]) if len(sys.argv) > 1 else 100
    content_size_kb = int(sys.argv[2]) if len(sys.argv) > 2 else 100
    pattern_density = float(sys.argv[3]) if len(sys.argv) > 3 else 0.1

    print(f"Generating test data:")
    print(f"  - Patterns: {num_patterns}")
    print(f"  - Content size: {content_size_kb} KB")
    print(f"  - Pattern density: {pattern_density * 100}%")
    print()

    # Generate and save patterns
    patterns = generate_patterns(num_patterns)
    save_patterns_file(patterns, "patterns.txt")

    # Generate and save HTML content
    html_content = generate_html_content(content_size_kb, pattern_density)
    save_html_file(html_content, "test_content.html")

    # Generate multiple sizes for comprehensive testing
    for size in [10, 50, 100, 500]:
        html = generate_html_content(size, pattern_density)
        save_html_file(html, f"test_content_{size}kb.html")

    print("\nTest data generation complete!")

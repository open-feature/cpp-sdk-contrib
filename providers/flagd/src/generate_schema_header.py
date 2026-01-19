#!/usr/bin/env python3
import argparse
import os

def main():
    parser = argparse.ArgumentParser(description='Generate embedded schemas header.')
    parser.add_argument('--output', required=True, help='Output header file')
    parser.add_argument('inputs', nargs='+', help='Input schema files')

    args = parser.parse_args()

    # Ensure output directory exists
    output_dir = os.path.dirname(args.output)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    with open(args.output, 'w') as f:
        f.write('#ifndef FLAGD_JSON_EMBEDDED_SCHEMAS_H_\n')
        f.write('#define FLAGD_JSON_EMBEDDED_SCHEMAS_H_\n\n')
        f.write('#include <map>\n')
        f.write('#include <string>\n\n')
        f.write('namespace schema {\n\n')
        f.write('static const std::map<std::string, std::string> schemas = {\n')

        for input_path in args.inputs:
            name = os.path.basename(input_path)
            with open(input_path, 'r') as schema_file:
                content = schema_file.read()
                # Using R"raw(...)raw" for C++ raw string literal
                f.write(f'    {{"{name}", R"raw({content})raw"}},\n')

        f.write('};\n\n')
        f.write('}  // namespace schema\n\n')
        f.write('#endif  // FLAGD_JSON_EMBEDDED_SCHEMAS_H_\n')

if __name__ == "__main__":
    main()

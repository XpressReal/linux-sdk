import http.server
import socketserver
import json
import os
import sys
import yaml
import re
import subprocess

PORT = 8766
HOST = '127.0.0.1'  # Development board IP
YAML_FILE = '/etc/kvmd/override.yaml'


class WolHandler(http.server.BaseHTTPRequestHandler):
    def do_OPTIONS(self):
        # Handle CORS preflight request
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()

    def do_GET(self):
        if self.path == '/wol':
            try:
                with open(YAML_FILE, 'r') as f:
                    data = yaml.safe_load(f)
                mac = data['kvmd']['wol']['mac']
                self.send_response(200)
                self.send_header('Content-type', 'text/plain')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(mac.encode('utf-8'))
            except Exception as e:
                print(f"Error processing GET: {e}")
                self.send_response(500)
                self.send_header('Content-type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps({'error': str(e)}).encode('utf-8'))
        else:
            self.send_response(404)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()

    def do_POST(self):
        if self.path == '/wol':
            try:
                if not os.path.exists(YAML_FILE):
                    raise FileNotFoundError(f"YAML file not found: {YAML_FILE}")

                content_length = int(self.headers['Content-Length'])
                post_data = self.rfile.read(content_length)
                data = json.loads(post_data)
                new_mac = data.get('mac', '').strip()
                if not new_mac:
                    raise ValueError("No 'mac' in POST body")

                # Read current MAC using yaml parsing to ensure keys exist
                with open(YAML_FILE, 'r') as f:
                    yaml_data = yaml.safe_load(f) or {}

                if 'kvmd' not in yaml_data:
                    yaml_data['kvmd'] = {}
                if 'wol' not in yaml_data['kvmd']:
                    yaml_data['kvmd']['wol'] = {}

                current_mac = yaml_data['kvmd']['wol'].get('mac', '').strip()

                updated = False
                if new_mac != current_mac:
                    with open(YAML_FILE, 'r') as f:
                        content = f.read()

                    pattern = r'(wol:\s*\n\s*mac:\s*)([^#\n\r]+?)(?=\s*#|$)'
                    replacement = r'\1' + new_mac
                    new_content = re.sub(pattern, replacement, content, flags=re.MULTILINE)

                    if 'wol:' not in new_content:
                        new_content += '\n\nwol:\n  mac: ' + new_mac + '\n'

                    with open(YAML_FILE, 'w') as f:
                        f.write(new_content)

                    updated = True
                    print(f"Updated MAC from '{current_mac}' to '{new_mac}' (preserved comments)")

                    result = subprocess.run(
                        ['systemctl', 'restart', 'kvmd'],
                        capture_output=True,
                        text=True
                    )
                    if result.returncode != 0:
                        print(f"Warning: kvmd restart failed - {result.stderr}")

                if updated:
                    response = {'status': 'restartKvm', 'updated': updated}
                else:
                    response = {'status': 'ok', 'updated': updated}

                self.send_response(200)
                self.send_header('Content-type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps(response).encode('utf-8'))

            except KeyError as e:
                print(f"KeyError in POST: {e}")
                self.send_response(400)
                self.send_header('Content-type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps({'error': f'Missing key: {str(e)}'}).encode('utf-8'))

            except FileNotFoundError as e:
                print(f"FileError in POST: {e}")
                self.send_response(404)
                self.send_header('Content-type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps({'error': str(e)}).encode('utf-8'))

            except Exception as e:
                print(f"Unexpected error in POST: {e}")
                self.send_response(400)
                self.send_header('Content-type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps({'error': str(e)}).encode('utf-8'))
        else:
            self.send_response(404)
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()


class ReusableTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


if __name__ == "__main__":
    print("Device check skipped for this implementation.")
    print(f"HTTP server started on http://{HOST}:{PORT}")
    try:
        with ReusableTCPServer((HOST, PORT), WolHandler) as httpd:
            httpd.serve_forever()
    except KeyboardInterrupt:
        print("Shutting down server")
        httpd.server_close()

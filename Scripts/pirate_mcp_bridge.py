#!/usr/bin/env python3
"""
PirateMCP Bridge — MCP stdio <-> TCP bridge for Claude Code
Translates MCP JSON-RPC (stdin/stdout) to PirateMCP TCP server (port 55558).

Usage:
  python pirate_mcp_bridge.py

Environment:
  PIRATE_MCP_PORT  - TCP port (default: 55558)
  PIRATE_MCP_HOST  - TCP host (default: 127.0.0.1)
"""

import sys
import json
import socket
import os
import time
import threading
import logging

# ─── Configuration ────────────────────────────────────────────────────────────
HOST = os.environ.get("PIRATE_MCP_HOST", "127.0.0.1")
PORT = int(os.environ.get("PIRATE_MCP_PORT", "55558"))
CONNECT_TIMEOUT = 5.0
RECV_TIMEOUT    = 30.0

logging.basicConfig(
    level=logging.DEBUG,
    format="[pirate_mcp_bridge] %(levelname)s %(message)s",
    stream=sys.stderr,
)
log = logging.getLogger("bridge")

# ─── Tool definitions exposed to Claude Code ──────────────────────────────────
TOOLS = [
    # ── Core ──
    {"name": "ping",
     "description": "Ping the PirateMCP server. Returns pong + api_version.",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},

    # ── Editor / Actor ──
    {"name": "get_actors_in_level",
     "description": "List all actors in the current level.",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "find_actors_by_name",
     "description": "Find actors whose name contains 'pattern'.",
     "inputSchema": {"type": "object",
                     "properties": {"pattern": {"type": "string"}},
                     "required": ["pattern"]}},
    {"name": "spawn_actor",
     "description": "Spawn an actor in the editor world.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "type": {"type": "string", "description": "StaticMeshActor|PointLight|SpotLight|DirectionalLight|CameraActor"},
                         "name": {"type": "string"},
                         "location": {"type": "array", "items": {"type": "number"}, "description": "[x,y,z]"},
                         "rotation": {"type": "array", "items": {"type": "number"}, "description": "[pitch,yaw,roll]"},
                         "scale":    {"type": "array", "items": {"type": "number"}, "description": "[x,y,z]"},
                         "static_mesh": {"type": "string", "description": "Optional asset path"},
                     }, "required": ["type", "name"]}},
    {"name": "delete_actor",
     "description": "Delete an actor by name.",
     "inputSchema": {"type": "object",
                     "properties": {"name": {"type": "string"}},
                     "required": ["name"]}},
    {"name": "set_actor_transform",
     "description": "Move/rotate/scale an actor.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "name":     {"type": "string"},
                         "location": {"type": "array", "items": {"type": "number"}},
                         "rotation": {"type": "array", "items": {"type": "number"}},
                         "scale":    {"type": "array", "items": {"type": "number"}},
                     }, "required": ["name"]}},
    {"name": "actor_get_by_class",
     "description": "Find all actors of a given class name.",
     "inputSchema": {"type": "object",
                     "properties": {"class_name": {"type": "string"}},
                     "required": ["class_name"]}},
    {"name": "actor_set_collision_profile",
     "description": "Set collision profile on all primitive components of an actor.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "name":         {"type": "string"},
                         "profile_name": {"type": "string"},
                     }, "required": ["name", "profile_name"]}},
    {"name": "actor_set_material_scalar_param",
     "description": "Set a scalar material parameter on an actor.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "name":       {"type": "string"},
                         "param_name": {"type": "string"},
                         "value":      {"type": "number"},
                     }, "required": ["name", "param_name", "value"]}},
    {"name": "actor_add_niagara_component",
     "description": "Add a Niagara component to an existing actor.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "name":       {"type": "string"},
                         "asset_path": {"type": "string"},
                     }, "required": ["name", "asset_path"]}},

    # ── Blueprint ──
    {"name": "create_blueprint",
     "description": "Create a new Blueprint asset.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "name":        {"type": "string"},
                         "parent_class":{"type": "string", "description": "e.g. Actor, Pawn, Character"},
                         "path":        {"type": "string", "description": "e.g. /Game/Blueprints"},
                     }, "required": ["name", "parent_class"]}},
    {"name": "compile_blueprint",
     "description": "Compile a Blueprint.",
     "inputSchema": {"type": "object",
                     "properties": {"name": {"type": "string", "description": "Blueprint name or path"}},
                     "required": ["name"]}},
    {"name": "spawn_blueprint_actor",
     "description": "Spawn a Blueprint actor in the editor.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "actor_name":     {"type": "string"},
                         "location": {"type": "array", "items": {"type": "number"}},
                         "rotation": {"type": "array", "items": {"type": "number"}},
                     }, "required": ["blueprint_name", "actor_name"]}},

    # ── Ocean ──
    {"name": "ocean_set_material_scalar",
     "description": "Set a scalar parameter on a material instance on an actor.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "actor_name": {"type": "string"},
                         "param_name": {"type": "string"},
                         "value":      {"type": "number"},
                     }, "required": ["actor_name", "param_name", "value"]}},
    {"name": "ocean_set_material_vector",
     "description": "Set a vector/color parameter on a material instance on an actor.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "actor_name": {"type": "string"},
                         "param_name": {"type": "string"},
                         "value": {"type": "array", "items": {"type": "number"}, "description": "[r,g,b] or [r,g,b,a]"},
                     }, "required": ["actor_name", "param_name", "value"]}},
    {"name": "ocean_spawn_niagara",
     "description": "Spawn a Niagara system at a world location.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "asset_path": {"type": "string"},
                         "location": {"type": "array", "items": {"type": "number"}},
                         "rotation": {"type": "array", "items": {"type": "number"}},
                     }, "required": ["asset_path"]}},
    {"name": "ocean_set_niagara_float",
     "description": "Set a float variable on a Niagara component of an actor.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "actor_name":    {"type": "string"},
                         "variable_name": {"type": "string"},
                         "value":         {"type": "number"},
                     }, "required": ["actor_name", "variable_name", "value"]}},
    {"name": "ocean_set_niagara_vector",
     "description": "Set a vector variable on a Niagara component of an actor.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "actor_name":    {"type": "string"},
                         "variable_name": {"type": "string"},
                         "value": {"type": "array", "items": {"type": "number"}, "description": "[x,y,z]"},
                     }, "required": ["actor_name", "variable_name", "value"]}},
    {"name": "ocean_list_water_bodies",
     "description": "List all WaterBody/Ocean actors in the current level.",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},

    # ── DataTable ──
    {"name": "datatable_list",
     "description": "List all DataTable assets in the project.",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "datatable_get_row_names",
     "description": "Get all row names from a DataTable.",
     "inputSchema": {"type": "object",
                     "properties": {"asset_path": {"type": "string"}},
                     "required": ["asset_path"]}},
    {"name": "datatable_get_row",
     "description": "Read a single row from a DataTable.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "asset_path": {"type": "string"},
                         "row_name":   {"type": "string"},
                     }, "required": ["asset_path", "row_name"]}},
    {"name": "datatable_get_all_rows",
     "description": "Read all rows from a DataTable.",
     "inputSchema": {"type": "object",
                     "properties": {"asset_path": {"type": "string"}},
                     "required": ["asset_path"]}},
    {"name": "datatable_set_row",
     "description": "Create or update a row in a DataTable.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "asset_path": {"type": "string"},
                         "row_name":   {"type": "string"},
                         "row_data":   {"type": "object"},
                     }, "required": ["asset_path", "row_name", "row_data"]}},
    {"name": "datatable_delete_row",
     "description": "Delete a row from a DataTable.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "asset_path": {"type": "string"},
                         "row_name":   {"type": "string"},
                     }, "required": ["asset_path", "row_name"]}},

    # ── World ──
    {"name": "world_get_info",
     "description": "Get current level name, actor count, and world partition status.",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "world_open_level",
     "description": "Open a level by asset path.",
     "inputSchema": {"type": "object",
                     "properties": {"level_path": {"type": "string"}},
                     "required": ["level_path"]}},
    {"name": "world_list_levels",
     "description": "List all .umap (World) assets in the project.",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "world_set_editor_camera",
     "description": "Teleport the editor viewport camera to a location/rotation.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "location": {"type": "array", "items": {"type": "number"}, "description": "[x,y,z]"},
                         "rotation": {"type": "array", "items": {"type": "number"}, "description": "[pitch,yaw,roll]"},
                     }, "required": ["location"]}},
    {"name": "world_list_streaming_levels",
     "description": "List all streaming sublevels and their load state.",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "world_load_streaming_level",
     "description": "Load a streaming sublevel by package name.",
     "inputSchema": {"type": "object",
                     "properties": {"package_name": {"type": "string"}},
                     "required": ["package_name"]}},
    {"name": "world_unload_streaming_level",
     "description": "Unload a streaming sublevel by package name.",
     "inputSchema": {"type": "object",
                     "properties": {"package_name": {"type": "string"}},
                     "required": ["package_name"]}},
]


# ─── TCP connection to UE plugin ───────────────────────────────────────────────
class TCPConnection:
    def __init__(self):
        self.sock = None
        self._lock = threading.Lock()

    def connect(self):
        for attempt in range(3):
            try:
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(CONNECT_TIMEOUT)
                s.connect((HOST, PORT))
                s.settimeout(RECV_TIMEOUT)
                self.sock = s
                log.info(f"Connected to PirateMCP on {HOST}:{PORT}")
                return True
            except OSError as e:
                log.warning(f"Connect attempt {attempt+1} failed: {e}")
                time.sleep(1.0)
        return False

    def send_command(self, command_type: str, params: dict) -> dict:
        with self._lock:
            if self.sock is None:
                if not self.connect():
                    return {"success": False, "error": f"Cannot connect to PirateMCP on {HOST}:{PORT}. Is UE Editor running with the plugin loaded?"}

            msg = json.dumps({"command": command_type, "params": params}) + "\n"
            try:
                self.sock.sendall(msg.encode("utf-8"))
            except OSError:
                self.sock = None
                if not self.connect():
                    return {"success": False, "error": "Lost connection to PirateMCP; reconnect failed."}
                self.sock.sendall(msg.encode("utf-8"))

            # Read until newline
            buf = b""
            while b"\n" not in buf:
                chunk = self.sock.recv(65536)
                if not chunk:
                    self.sock = None
                    return {"success": False, "error": "Connection closed by PirateMCP server."}
                buf += chunk

            line = buf.split(b"\n")[0]
            try:
                return json.loads(line.decode("utf-8"))
            except json.JSONDecodeError as e:
                return {"success": False, "error": f"Invalid JSON from server: {e}"}


tcp = TCPConnection()


# ─── MCP message handling ──────────────────────────────────────────────────────
def handle_initialize(msg_id, params):
    return {
        "jsonrpc": "2.0",
        "id": msg_id,
        "result": {
            "protocolVersion": "2024-11-05",
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "pirate-mcp", "version": "1.0"},
        }
    }


def handle_tools_list(msg_id, params):
    return {
        "jsonrpc": "2.0",
        "id": msg_id,
        "result": {"tools": TOOLS}
    }


def handle_tools_call(msg_id, params):
    tool_name  = params.get("name", "")
    tool_input = params.get("arguments") or params.get("input") or {}

    log.debug(f"Tool call: {tool_name}  params={tool_input}")
    result = tcp.send_command(tool_name, tool_input)
    log.debug(f"Result: {result}")

    text = json.dumps(result, indent=2)
    return {
        "jsonrpc": "2.0",
        "id": msg_id,
        "result": {
            "content": [{"type": "text", "text": text}],
            "isError": not result.get("success", True),
        }
    }


def process_message(raw: str) -> dict | None:
    try:
        msg = json.loads(raw)
    except json.JSONDecodeError:
        return None

    method = msg.get("method", "")
    msg_id = msg.get("id")
    params = msg.get("params") or {}

    if method == "initialize":
        return handle_initialize(msg_id, params)
    if method == "initialized":
        return None  # notification, no response
    if method == "tools/list":
        return handle_tools_list(msg_id, params)
    if method == "tools/call":
        return handle_tools_call(msg_id, params)
    if method == "ping":
        return {"jsonrpc": "2.0", "id": msg_id, "result": {}}

    # Unknown method
    if msg_id is not None:
        return {
            "jsonrpc": "2.0",
            "id": msg_id,
            "error": {"code": -32601, "message": f"Method not found: {method}"}
        }
    return None


def main():
    log.info(f"PirateMCP Bridge starting. Target: {HOST}:{PORT}")
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        response = process_message(line)
        if response is not None:
            sys.stdout.write(json.dumps(response) + "\n")
            sys.stdout.flush()


if __name__ == "__main__":
    main()

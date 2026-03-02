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

    # ── Blueprint (more) ──
    {"name": "add_component_to_blueprint",
     "description": "Add a component to a Blueprint.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "component_type": {"type": "string"},
                         "component_name": {"type": "string"},
                     }, "required": ["blueprint_name", "component_type"]}},
    {"name": "set_physics_properties",
     "description": "Set physics properties on a Blueprint component.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "component_name": {"type": "string"},
                         "simulate_physics": {"type": "boolean"},
                     }, "required": ["blueprint_name"]}},
    {"name": "set_static_mesh_properties",
     "description": "Set static mesh on a Blueprint component.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "component_name": {"type": "string"},
                         "static_mesh": {"type": "string"},
                     }, "required": ["blueprint_name"]}},
    {"name": "set_mesh_material_color",
     "description": "Set material color on a mesh.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "color": {"type": "array", "items": {"type": "number"}},
                     }, "required": ["blueprint_name"]}},
    {"name": "get_available_materials",
     "description": "List available materials in the project.",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "apply_material_to_actor",
     "description": "Apply a material to an actor.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "actor_name": {"type": "string"},
                         "material_path": {"type": "string"},
                     }, "required": ["actor_name", "material_path"]}},
    {"name": "apply_material_to_blueprint",
     "description": "Apply a material to a Blueprint component.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "material_path": {"type": "string"},
                     }, "required": ["blueprint_name", "material_path"]}},
    {"name": "get_actor_material_info",
     "description": "Get material info from an actor.",
     "inputSchema": {"type": "object",
                     "properties": {"actor_name": {"type": "string"}},
                     "required": ["actor_name"]}},
    {"name": "get_blueprint_material_info",
     "description": "Get material info from a Blueprint.",
     "inputSchema": {"type": "object",
                     "properties": {"blueprint_name": {"type": "string"}},
                     "required": ["blueprint_name"]}},
    {"name": "read_blueprint_content",
     "description": "Read the full content of a Blueprint (variables, components, graphs).",
     "inputSchema": {"type": "object",
                     "properties": {"name": {"type": "string"}},
                     "required": ["name"]}},
    {"name": "analyze_blueprint_graph",
     "description": "Analyze the graph of a Blueprint (nodes, connections).",
     "inputSchema": {"type": "object",
                     "properties": {"name": {"type": "string"}},
                     "required": ["name"]}},
    {"name": "get_blueprint_variable_details",
     "description": "Get variable details from a Blueprint.",
     "inputSchema": {"type": "object",
                     "properties": {"name": {"type": "string"}},
                     "required": ["name"]}},
    {"name": "get_blueprint_function_details",
     "description": "Get function details from a Blueprint.",
     "inputSchema": {"type": "object",
                     "properties": {"name": {"type": "string"}},
                     "required": ["name"]}},

    # ── Blueprint Graph ──
    {"name": "add_blueprint_node",
     "description": "Add a node to a Blueprint graph.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "node_type": {"type": "string"},
                         "node_params": {"type": "object"},
                     }, "required": ["blueprint_name", "node_type"]}},
    {"name": "connect_nodes",
     "description": "Connect two nodes in a Blueprint graph.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "source_node_id": {"type": "string"},
                         "source_pin": {"type": "string"},
                         "target_node_id": {"type": "string"},
                         "target_pin": {"type": "string"},
                     }, "required": ["blueprint_name", "source_node_id", "source_pin", "target_node_id", "target_pin"]}},
    {"name": "create_variable",
     "description": "Create a variable in a Blueprint.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "variable_name": {"type": "string"},
                         "variable_type": {"type": "string"},
                     }, "required": ["blueprint_name", "variable_name", "variable_type"]}},
    {"name": "set_blueprint_variable_properties",
     "description": "Set properties of a Blueprint variable.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "variable_name": {"type": "string"},
                     }, "required": ["blueprint_name", "variable_name"]}},
    {"name": "add_event_node",
     "description": "Add an event node (BeginPlay, Tick, etc.) to a Blueprint.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "event_name": {"type": "string"},
                     }, "required": ["blueprint_name", "event_name"]}},
    {"name": "delete_node",
     "description": "Delete a node from a Blueprint graph.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "node_id": {"type": "string"},
                     }, "required": ["blueprint_name", "node_id"]}},
    {"name": "set_node_property",
     "description": "Set a property on a Blueprint node.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "node_id": {"type": "string"},
                         "property_name": {"type": "string"},
                         "property_value": {"type": "string"},
                     }, "required": ["blueprint_name", "node_id", "property_name", "property_value"]}},
    {"name": "create_function",
     "description": "Create a function in a Blueprint.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "function_name": {"type": "string"},
                     }, "required": ["blueprint_name", "function_name"]}},
    {"name": "add_function_input",
     "description": "Add an input parameter to a Blueprint function.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "function_name": {"type": "string"},
                         "param_name": {"type": "string"},
                         "param_type": {"type": "string"},
                     }, "required": ["blueprint_name", "function_name", "param_name", "param_type"]}},
    {"name": "add_function_output",
     "description": "Add an output parameter to a Blueprint function.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "function_name": {"type": "string"},
                         "param_name": {"type": "string"},
                         "param_type": {"type": "string"},
                     }, "required": ["blueprint_name", "function_name", "param_name", "param_type"]}},
    {"name": "delete_function",
     "description": "Delete a function from a Blueprint.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "function_name": {"type": "string"},
                     }, "required": ["blueprint_name", "function_name"]}},
    {"name": "rename_function",
     "description": "Rename a function in a Blueprint.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "blueprint_name": {"type": "string"},
                         "old_name": {"type": "string"},
                         "new_name": {"type": "string"},
                     }, "required": ["blueprint_name", "old_name", "new_name"]}},

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

    # ── Python (GOD COMMAND) ──
    {"name": "exec_python",
     "description": "Execute arbitrary Python code inside the UE5 editor's built-in Python interpreter. "
                    "This gives you access to the ENTIRE `unreal` module — thousands of classes and methods. "
                    "Use `import unreal` then call any UE5 Python API. Output is captured from print() and return values. "
                    "Examples: 'import unreal; print(unreal.EditorAssetLibrary.list_assets(\"/Game\"))' or "
                    "'import unreal; actors = unreal.EditorLevelLibrary.get_all_level_actors(); print(len(actors))'. "
                    "For multi-line scripts, separate with newlines. execution_mode can be 'execute_file' (default, multi-statement), "
                    "'execute_statement' (single statement, prints result), or 'evaluate_statement' (single expression, returns value).",
     "inputSchema": {"type": "object",
                     "properties": {
                         "code": {"type": "string", "description": "Python code to execute. Use 'import unreal' for UE5 APIs."},
                         "execution_mode": {"type": "string", "enum": ["execute_file", "execute_statement", "evaluate_statement"],
                                           "description": "How to execute the code. Default: execute_file"},
                     }, "required": ["code"]}},
    {"name": "exec_console_command",
     "description": "Execute a UE5 console command (e.g. 'stat fps', 'r.SetRes 1920x1080', 'obj list').",
     "inputSchema": {"type": "object",
                     "properties": {
                         "command": {"type": "string", "description": "Console command to execute"},
                     }, "required": ["command"]}},
    {"name": "get_python_help",
     "description": "Get Python API documentation for a UE5 class or method. "
                    "Use this to discover available APIs. "
                    "Examples: target='unreal.EditorAssetLibrary' help_type='dir' to list methods, "
                    "target='unreal.EditorAssetLibrary.list_assets' help_type='help' for full docs.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "target": {"type": "string", "description": "Python object to get help for (e.g. 'unreal.EditorAssetLibrary')"},
                         "help_type": {"type": "string", "enum": ["help", "dir", "signature"],
                                      "description": "Type of help: 'help' for docstring, 'dir' for member list, 'signature' for function signature"},
                     }, "required": ["target"]}},

    # ── Editor Utility ──
    {"name": "editor_undo",
     "description": "Undo the last editor action (Ctrl+Z equivalent).",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "editor_redo",
     "description": "Redo the last undone action (Ctrl+Y equivalent).",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "editor_save_all",
     "description": "Save all dirty/modified packages (maps, assets, etc.).",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "editor_play",
     "description": "Start Play In Editor (PIE) — runs the game in the editor viewport.",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "editor_stop",
     "description": "Stop Play In Editor (PIE).",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "editor_get_selected",
     "description": "Get the currently selected actors in the editor (names, classes, transforms).",
     "inputSchema": {"type": "object", "properties": {}, "required": []}},
    {"name": "editor_select_actors",
     "description": "Select actors in the editor by name.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "names": {"type": "array", "items": {"type": "string"}, "description": "Actor names to select"},
                     }, "required": ["names"]}},
    {"name": "editor_take_screenshot",
     "description": "Take a screenshot of the editor viewport and save to Saved/Screenshots/.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "filename": {"type": "string", "description": "Optional filename (default: auto-timestamped)"},
                     }, "required": []}},
    {"name": "editor_get_output_log",
     "description": "Get info about the output log directory.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "max_entries": {"type": "integer", "description": "Max log entries to return (default 50)"},
                     }, "required": []}},

    # ── Asset Management ──
    {"name": "asset_list",
     "description": "List assets in the project. Filter by path and/or class.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "path": {"type": "string", "description": "Content path to search (default: /Game)"},
                         "class_filter": {"type": "string", "description": "Filter by class name (e.g. 'StaticMesh', 'Material', 'Blueprint')"},
                         "recursive": {"type": "boolean", "description": "Search recursively (default: true)"},
                     }, "required": []}},
    {"name": "asset_get_info",
     "description": "Get detailed info about an asset (class, size, disk path, dirty state).",
     "inputSchema": {"type": "object",
                     "properties": {
                         "asset_path": {"type": "string", "description": "Content path of the asset (e.g. /Game/Pirate/Ships/BP_Ship)"},
                     }, "required": ["asset_path"]}},
    {"name": "asset_duplicate",
     "description": "Duplicate an asset to a new location.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "source_path": {"type": "string"},
                         "dest_path": {"type": "string"},
                     }, "required": ["source_path", "dest_path"]}},
    {"name": "asset_rename",
     "description": "Rename or move an asset to a new path.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "source_path": {"type": "string"},
                         "dest_path": {"type": "string"},
                     }, "required": ["source_path", "dest_path"]}},
    {"name": "asset_delete",
     "description": "Delete an asset from the project.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "asset_path": {"type": "string"},
                     }, "required": ["asset_path"]}},
    {"name": "asset_import",
     "description": "Import an external file (FBX, PNG, WAV, etc.) into the project.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "file_path": {"type": "string", "description": "Absolute path to the file on disk"},
                         "dest_path": {"type": "string", "description": "Content path to import into (e.g. /Game/Pirate/Meshes)"},
                     }, "required": ["file_path", "dest_path"]}},
    {"name": "asset_save",
     "description": "Save a specific asset or all dirty assets.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "asset_path": {"type": "string", "description": "Optional: specific asset to save. If omitted, saves all dirty."},
                     }, "required": []}},
    {"name": "asset_find_references",
     "description": "Find all assets that reference a given asset.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "asset_path": {"type": "string"},
                     }, "required": ["asset_path"]}},

    # ── Actor Properties ──
    {"name": "actor_get_properties",
     "description": "Get all editable UPROPERTY values from an actor (name, type, value, category).",
     "inputSchema": {"type": "object",
                     "properties": {
                         "name": {"type": "string", "description": "Actor name"},
                     }, "required": ["name"]}},
    {"name": "actor_set_property",
     "description": "Set a UPROPERTY value on an actor by property name.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "name": {"type": "string", "description": "Actor name"},
                         "property_name": {"type": "string", "description": "Property name (e.g. 'bHidden', 'Mobility')"},
                         "value": {"type": "string", "description": "Value as text (UE text export format)"},
                     }, "required": ["name", "property_name", "value"]}},
    {"name": "actor_get_components",
     "description": "List all components on an actor (name, class, location, parent).",
     "inputSchema": {"type": "object",
                     "properties": {
                         "name": {"type": "string", "description": "Actor name"},
                     }, "required": ["name"]}},
    {"name": "actor_add_component",
     "description": "Add a component to an actor by class name.",
     "inputSchema": {"type": "object",
                     "properties": {
                         "name": {"type": "string", "description": "Actor name"},
                         "component_class": {"type": "string", "description": "Component class (e.g. StaticMeshComponent, PointLightComponent, AudioComponent)"},
                     }, "required": ["name", "component_class"]}},
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

@echo off
REM PirateMCP Bridge launcher
REM Starts the MCP stdio^<->TCP bridge that connects Claude Code to the UE5 PirateMCP plugin.
REM The UE5 editor must be running with the PirateMCP plugin loaded (port 55558).

python "%~dp0pirate_mcp_bridge.py" %*

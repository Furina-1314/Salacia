#!/usr/bin/env python3
"""Run commands on the ROV board via SSH. Usage: sshboard.py "cmd1" ["cmd2" ...]"""
import sys
import paramiko

HOST = "192.168.1.120"
USER = "root"
PASSWORD = "123456"


def main() -> int:
    commands = sys.argv[1:]
    if not commands:
        print("no command given", file=sys.stderr)
        return 2
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, username=USER, password=PASSWORD, timeout=10,
                   banner_timeout=15, auth_timeout=15, look_for_keys=False,
                   allow_agent=False)
    try:
        for command in commands:
            print(f"$ {command}")
            stdin, stdout, stderr = client.exec_command(
                command, timeout=int(__import__("os").environ.get("BOARD_CMD_TIMEOUT", "300")))
            out = stdout.read().decode("utf-8", "replace")
            err = stderr.read().decode("utf-8", "replace")
            rc = stdout.channel.recv_exit_status()
            if out:
                print(out.rstrip())
            if err:
                print("[stderr]", err.rstrip())
            print(f"[rc={rc}]")
            print("-" * 60)
    finally:
        client.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())

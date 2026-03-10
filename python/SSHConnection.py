import paramiko
from CommandResult import CommandResult

class SSHConnection:
    """Manages a remote SSH session using a context manager."""
    def __init__(self, host: str, user: str, key_path: str):
        self.host = host
        self.user = user
        self.key_path = key_path
        self.client = paramiko.SSHClient()
        self.client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

    def __enter__(self):
        """Opens the connection when entering the 'with' block."""
        self.client.connect(self.host, username=self.user, key_filename=self.key_path)
        return self

    def execute(self, command: str) -> CommandResult:
        """Runs a command on the remote host."""
        stdin, stdout, stderr = self.client.exec_command(command)
        return CommandResult(
            stdout=stdout.read().decode(),
            stderr=stderr.read().decode(),
            returncode=stdout.channel.recv_exit_status()
        )

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Ensures the connection is closed automatically."""
        self.client.close()

def main():
    try:
        with SSHConnection(host="192.168.1.10", user="user", key_path="~/.ssh/id_rsa") as conn:
            result = conn.execute("ls -la")
            print(result.to_dict())
    except Exception as e:
        print(f"Failed to execute command: {e}")

if __name__ == "__main__":
    main()
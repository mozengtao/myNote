import os
import subprocess
import json
import paramiko
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Union

# --- 1. The Result Object ---
class CommandResult:
    def __init__(self, stdout: str, stderr: str, returncode: int):
        self.stdout = stdout.strip()
        self.stderr = stderr.strip()
        self.returncode = returncode

    @property
    def is_success(self) -> bool:
        return self.returncode == 0

    def to_json(self):
        try:
            return json.loads(self.stdout)
        except json.JSONDecodeError:
            return None

# --- 2. The Environment Validator ---
@dataclass
class EnvironmentConfig:
    required_vars: List[str]
    values: Dict[str, str] = field(default_factory=dict, init=False)

    def __post_init__(self):
        for var in self.required_vars:
            val = os.environ.get(var)
            if not val:
                raise EnvironmentError(f"Missing Critical Env Var: {var}")
            self.values[var] = val

# --- 3. The Remote Connection Manager ---
class SSHConnection:
    def __init__(self, host: str, user: str, pkey_path: str):
        self.host, self.user, self.pkey_path = host, user, pkey_path
        self.client = paramiko.SSHClient()
        self.client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

    def __enter__(self):
        self.client.connect(self.host, username=self.user, key_filename=self.pkey_path)
        return self

    def execute(self, cmd: str) -> CommandResult:
        _, stdout, stderr = self.client.exec_command(cmd)
        return CommandResult(
            stdout.read().decode(), 
            stderr.read().decode(), 
            stdout.channel.recv_exit_status()
        )

    def __exit__(self, *args):
        self.client.close()

# --- 3. The File Transfer Manager ---
class FileTransfer:
    """Handles uploading and downloading files via SFTP."""
    def __init__(self, ssh_client: paramiko.SSHClient):
        # We reuse the existing transport from an active SSH session
        self.sftp = ssh_client.open_sftp()

    def upload(self, local_path: str, remote_path: str):
        """Uploads a local file to the remote server."""
        print(f"Uploading {local_path} -> {remote_path}")
        self.sftp.put(local_path, remote_path)

    def download(self, remote_path: str, local_path: str):
        """Downloads a remote file to the local machine."""
        print(f"Downloading {remote_path} -> {local_path}")
        self.sftp.get(remote_path, local_path)

    def close(self):
        self.sftp.close()

# --- 4. The Orchestrator (The "Brain") ---
class TaskOrchestrator:
    def __init__(self, config: EnvironmentConfig):
        self.config = config

    def deploy_and_run(self, host: str, local_script: str):
        user = self.config.get("REMOTE_USER")
        key = self.config.get("SSH_KEY_PATH")
        remote_path = f"/tmp/{os.path.basename(local_script)}"

        try:
            # 1. Establish Connection
            with SSHConnection(host, user, key) as conn:

                # 2. Transfer the script
                transfer = FileTransfer(conn.client)
                transfer.upload(local_script, remote_path)

                # 3. Make script executable & Run it
                conn.execute(f"chmod +x {remote_path}")
                result = conn.execute(remote_path)

                # 4. Handle the Result
                if result.is_success:
                    print(f"Deployment Successful! Output:\n{result.stdout}")
                else:
                    print(f"Deployment Failed: {result.stderr}")

                # 5. Cleanup
                conn.execute(f"rm {remote_path}")
                transfer.close()

        except Exception as e:
            print(f"Critical Orchestration Error: {e}")

# --- USAGE ---
if __name__ == "__main__":
    # 1. Setup Environment (This would fail if these aren't in your shell)
    # For this demo, let's manually set them:
    os.environ["REMOTE_USER"] = "ubuntu"
    os.environ["SSH_KEY_PATH"] = "/home/user/.ssh/id_rsa"
    
    env_manager = EnvironmentConfig(required_vars=["REMOTE_USER", "SSH_KEY_PATH"])

    # 2. Inject config into the Orchestrator
    orchestrator = TaskOrchestrator(env_manager)

    # 3. Execute
    # orchestrator.run_health_check("10.0.0.50")

    # Path to a local shell script you want to run remotely
    orchestrator.deploy_and_run("192.168.1.50", "./deploy.sh")

"""
Why this architecture wins:
Decoupling: The TaskOrchestrator doesn't care how the SSH connection works; it just knows it can call .execute().
Error Handling: If EnvironmentConfig fails, the script dies before trying to connect to a server with a null username.
Readability: The with block in the orchestrator makes the lifecycle of the network socket crystal clear.
"""
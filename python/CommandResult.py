import json

class CommandResult:
    """A refined wrapper for command execution output."""
    def __init__(self, stdout: str, stderr: str, returncode: int):
        self.stdout = stdout.strip()
        self.stderr = stderr.strip()
        self.returncode = returncode

    @property
    def is_success(self) -> bool:
        return self.returncode == 0

    def to_dict(self) -> dict:
        """Attempts to parse stdout as JSON; returns empty dict on failure."""
        try:
            return json.loads(self.stdout)
        except json.JSONDecodeError:
            return {}

    def __repr__(self):
        status = "SUCCESS" if self.is_success else f"FAILED({self.returncode})"
        return f"<CommandResult status={status}>"

def main():
    try:
        # Assuming 'res' came from your ShellExecutor
        result = CommandResult(stdout='{"status": "success"}', stderr="", returncode=0)
        if result.is_success:
            print(result.to_dict())
        else:
            print(f"Command failed with return code {result.returncode}")
            print(f"Error output: {result.stderr}")
    except Exception as e:
        print(f"Failed to get command result: {e}")

if __name__ == "__main__":
    main()
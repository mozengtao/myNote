import subprocess
import logging
from typing import Optional, List, Union

class ShellExecutor:
    """
    An OOP wrapper for executing bash commands and scripts.
    """

    def __init__(self, capture_output: bool = True, timeout: int = 3600):
        self.capture_output = capture_output
        self.timeout = timeout
        # Set up basic logging
        logging.basicConfig(level=logging.INFO)
        self.logger = logging.getLogger("ShellExecutor")

    def run(self, command: Union[str, List[str]], capture: Optional[bool] = None) -> subprocess.CompletedProcess:
        """
        Executes a command. Use capture=True to return stdout/stderr.
        """
        should_capture = capture if capture is not None else self.capture_output

        # If command is a string and has pipes/redirects, we use shell=True
        use_shell = isinstance(command, str)

        try:
            result = subprocess.run(
                command,
                shell=use_shell,
                capture_output=should_capture,
                text=True,  # Returns strings instead of bytes
                timeout=self.timeout,
                check=True  # Raises CalledProcessError if return code != 0
            )
            return result

        except subprocess.CalledProcessError as e:
            self.logger.error(f"Command failed with return code {e.returncode}")
            self.logger.error(f"Error output: {e.stderr}")
            raise
        except subprocess.TimeoutExpired:
            self.logger.error(f"Command timed out after {self.timeout} seconds")
            raise

    def run_script(self, script_path: str, args: List[str] = None) -> subprocess.CompletedProcess:
        """
        Executes a .sh file.
        """
        full_command = ["bash", script_path]
        if args:
            full_command.extend(args)

        return self.run(full_command, capture=self.capture_output)

def main():
    executor = ShellExecutor(capture_output=True)
    try:
        result = executor.run("ls -la")
        print(f"Output:\n{result.stdout}")

        result = executor.run_script("./deploy.sh", args=["prod", "--force"])
        print(f"Output:\n{result.stdout}")
    except Exception as e:
        print(f"Failed to execute command: {e}")

if __name__ == "__main__":
    main()
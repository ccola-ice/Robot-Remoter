"""Host-test include paths shared by the application modules."""
from pathlib import Path


def app_include_args():
    app = Path(__file__).resolve().parents[2] / "1_App"
    directories = ("", "config", "control", "protocol", "ui", "files", "diagnostics", "system")
    return [argument for directory in directories for argument in ("-I", str(app / directory))]

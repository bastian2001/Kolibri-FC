Import("env")
import subprocess
from pathlib import Path

# Paths for the header file and the temporary file to store the last hash
output_file = Path("include/git_version.h")
temp_file = Path(".last_git_hash")

def get_git_hash():
    """Get the current Git hash."""
    try:
        git_hash = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).strip().decode()
        return git_hash
    except Exception as e:
        print("Error fetching Git hash:", e)
        return "unknown"

def main(env):
    print("Updating Git hash in header file...")
    current_hash = get_git_hash()
    
    # Check if the last hash matches rexrthe current hash
    if temp_file.exists():
        last_hash = temp_file.read_text().strip()
        if last_hash == current_hash:
            print("Git hash unchanged; skipping header update.")
            return

    # Update the temp file with the current hash
    temp_file.write_text(current_hash)

    # Write the new git_version.h file
    content = f'''#ifndef GIT_VERSION_H
#define GIT_VERSION_H
#define GIT_HASH "{current_hash}"
#endif
'''
    output_file.write_text(content)
    print(f"Updated {output_file} with Git hash: {current_hash}")

main(env['PIOENV'])

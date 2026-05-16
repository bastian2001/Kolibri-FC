# Copyright (c) 2026 Kolibri-FC contributors
# 
# This file is part of Kolibri-FC (https://github.com/bastian2001/Kolibri-FC).
# 
# Kolibri-FC is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# Kolibri-FC is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with Kolibri-FC. If not, see <http://www.gnu.org/licenses/>.

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

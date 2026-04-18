#!/usr/bin/env python3
import os
import sys
import subprocess
import json
import urllib.request
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext, simpledialog
import re # Import re module for regex operations

class GitManagerGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("UCCC Git Management Console")
        self.root.geometry("900x700")
        
        # Set the repository path
        self.repo_path = os.getcwd()
        self.github_user = "vamps-goes-coding"
        self.github_repo = "UnifiedConkyControlCenter"
        
        # Verify path exists
        if not os.path.exists(self.repo_path):
            messagebox.showerror("Error", f"Repository path not found:\n{self.repo_path}")
            self.root.destroy()
            return

        self.setup_ui()
        self.log_message(f"Initialized Git Helper for: {self.repo_path}")
        self.refresh_status()

    def setup_ui(self):
        # Main Layout: Tabs at top, Console at bottom
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(expand=True, fill='both', padx=10, pady=5)

        # --- Tab 1: Status & Changes ---
        self.tab_status = ttk.Frame(self.notebook)
        self.notebook.add(self.tab_status, text=" Status & Diffs ")
        
        status_btn_frame = ttk.Frame(self.tab_status)
        status_btn_frame.pack(fill='x', padx=5, pady=5)
        ttk.Button(status_btn_frame, text="Refresh Status", command=self.refresh_status).pack(side='left', padx=2)
        ttk.Button(status_btn_frame, text="View Diff", command=self.view_diff).pack(side='left', padx=2)

        self.status_display = scrolledtext.ScrolledText(self.tab_status, height=20, font=("monospace", 10))
        self.status_display.pack(expand=True, fill='both', padx=5, pady=5)

        # --- Tab 2: Commit ---
        self.tab_commit = ttk.Frame(self.notebook)
        self.notebook.add(self.tab_commit, text=" Commit Changes ")
        
        ttk.Label(self.tab_commit, text="Commit Message:", font=("Arial", 10, "bold")).pack(anchor='w', padx=10, pady=(10, 2))
        self.commit_msg_entry = tk.Text(self.tab_commit, height=5)
        self.commit_msg_entry.pack(fill='x', padx=10, pady=5)
        
        commit_btn_frame = ttk.Frame(self.tab_commit)
        commit_btn_frame.pack(fill='x', padx=10, pady=5)
        ttk.Button(commit_btn_frame, text="Stage All (git add .)", command=self.stage_all).pack(side='left', padx=2)
        ttk.Button(commit_btn_frame, text="Commit", command=self.commit_changes).pack(side='left', padx=2)

        # --- Tab 3: Sync (Push/Pull) ---
        self.tab_sync = ttk.Frame(self.notebook)
        self.notebook.add(self.tab_sync, text=" Sync (Remote) ")
        
        sync_frame = ttk.LabelFrame(self.tab_sync, text="Remote Actions")
        sync_frame.pack(fill='both', expand=True, padx=20, pady=20)
        
        ttk.Button(sync_frame, text="Fetch (Update Local Info)", command=lambda: self.run_git(["fetch"])).pack(pady=10, ipadx=20)
        ttk.Button(sync_frame, text="Pull (Get Latest from GitHub)", command=lambda: self.run_git(["pull"])).pack(pady=10, ipadx=20)
        ttk.Button(sync_frame, text="Push (Upload to GitHub)", command=lambda: self.run_git(["push"])).pack(pady=10, ipadx=20)

        # --- Tab 4: Branching ---
        self.tab_branch = ttk.Frame(self.notebook)
        self.notebook.add(self.tab_branch, text=" Branches ")
        
        branch_input_frame = ttk.Frame(self.tab_branch)
        branch_input_frame.pack(fill='x', padx=10, pady=10)
        
        ttk.Label(branch_input_frame, text="Branch Name:").pack(side='left')
        self.branch_name_var = tk.StringVar()
        ttk.Entry(branch_input_frame, textvariable=self.branch_name_var).pack(side='left', padx=5, fill='x', expand=True)
        
        ttk.Button(branch_input_frame, text="Switch/Checkout", command=self.switch_branch).pack(side='left', padx=2)
        ttk.Button(branch_input_frame, text="Create New", command=self.create_branch).pack(side='left', padx=2)
        
        self.branch_list_display = scrolledtext.ScrolledText(self.tab_branch, height=15)
        self.branch_list_display.pack(expand=True, fill='both', padx=10, pady=10)
        ttk.Button(self.tab_branch, text="List All Branches", command=self.list_branches).pack(pady=5)

        # --- Tab 5: Versioning & Releases ---
        self.tab_version = ttk.Frame(self.notebook)
        self.notebook.add(self.tab_version, text=" Versioning & Tags ")

        ver_info_frame = ttk.LabelFrame(self.tab_version, text="Version Info")
        ver_info_frame.pack(fill='x', padx=20, pady=10)

        self.lbl_github_ver = ttk.Label(ver_info_frame, text="Latest on GitHub: Fetching...", font=("Arial", 10, "bold"))
        self.lbl_github_ver.pack(anchor='w', padx=10, pady=5)

        self.lbl_local_tag = ttk.Label(ver_info_frame, text="Latest Local Tag: Loading...")
        self.lbl_local_tag.pack(anchor='w', padx=10, pady=5)

        ttk.Button(ver_info_frame, text="Refresh Version Info", command=self.refresh_version_data).pack(pady=5)

        tag_action_frame = ttk.LabelFrame(self.tab_version, text="Push New Version")
        tag_action_frame.pack(fill='x', padx=20, pady=10)

        ttk.Label(tag_action_frame, text="New Tag (e.g. v1.0.46):").pack(anchor='w', padx=10, pady=2)
        self.new_tag_var = tk.StringVar()
        ttk.Entry(tag_action_frame, textvariable=self.new_tag_var).pack(fill='x', padx=10, pady=5)

        ttk.Label(tag_action_frame, text="Tag Message:").pack(anchor='w', padx=10, pady=2)
        self.tag_msg_entry = ttk.Entry(tag_action_frame)
        self.tag_msg_entry.pack(fill='x', padx=10, pady=5)

        tag_btn_row = ttk.Frame(tag_action_frame)
        tag_btn_row.pack(fill='x', padx=10, pady=10)
        
        ttk.Button(tag_btn_row, text="Create Local Tag", 
                   command=self.create_local_tag).pack(side='left', padx=5)
        
        push_tag_btn = ttk.Button(tag_btn_row, text="Push Tag to GitHub", 
                                  command=self.push_tag_to_remote)
        push_tag_btn.pack(side='left', padx=5)
        
        tag_help = ttk.Label(tag_action_frame, text="Note: Pushing a tag notifies GitHub to create a release entry.", font=("Arial", 8, "italic"))
        tag_help.pack(padx=10, pady=5)

        # --- Bottom Console Output ---
        console_frame = ttk.LabelFrame(self.root, text="Output Console")
        console_frame.pack(fill='x', side='bottom', padx=10, pady=10)
        
        self.console = scrolledtext.ScrolledText(console_frame, height=8, bg="#1e1e1e", fg="#dcdcdc", font=("monospace", 9))
        self.console.pack(fill='x', padx=5, pady=5)

    # --- Git Logic ---

    def run_git(self, args):
        """Handles execution of Git commands and captures output/errors."""
        cmd = ["git"] + args
        # Set up environment to use this script as an ASKPASS helper for SSH/Git
        env = os.environ.copy()
        script_path = os.path.abspath(__file__)
        env["GIT_ASKPASS"] = script_path
        env["SSH_ASKPASS"] = script_path
        env["SSH_ASKPASS_REQUIRE"] = "force"

        try:
            # Run process in the specific repo directory
            result = subprocess.run(
                cmd, 
                cwd=self.repo_path, 
                capture_output=True, 
                text=True, 
                check=False,
                env=env
            )
            
            if result.returncode == 0:
                self.log_message(f"Success: git {' '.join(args)}", "green")
                if result.stdout:
                    self.log_message(result.stdout)
                return result.stdout
            else:
                self.log_message(f"Error executing: git {' '.join(args)}", "red")
                self.log_message(result.stderr, "red")
                messagebox.showwarning("Git Warning", result.stderr)
                return None
        except Exception as e:
            self.log_message(f"System Failure: {str(e)}", "red")
            return None

    def log_message(self, message, color=None):
        self.console.insert(tk.END, message + "\n")
        # Note: In a real app we'd add tags for colors, but keeping it simple for now
        self.console.see(tk.END)

    def refresh_status(self):
        output = self.run_git(["status"])
        if output:
            self.status_display.config(state='normal')
            self.status_display.delete('1.0', tk.END)
            self.status_display.insert(tk.END, output)
            self.status_display.config(state='disabled')
        self.list_branches()
        self.refresh_version_data()

    def view_diff(self):
        output = self.run_git(["diff"])
        if output:
            self.status_display.config(state='normal')
            self.status_display.delete('1.0', tk.END)
            if not output.strip():
                self.status_display.insert(tk.END, "No unstaged changes to display.")
            else:
                self.status_display.insert(tk.END, output)
            self.status_display.config(state='disabled')

    def stage_all(self):
        self.run_git(["add", "."])
        self.refresh_status()

    def commit_changes(self):
        msg = self.commit_msg_entry.get("1.0", tk.END).strip()
        if not msg:
            messagebox.showwarning("Input Needed", "Please enter a commit message!")
            return
        
        res = self.run_git(["commit", "-m", msg])
        if res:
            self.commit_msg_entry.delete("1.0", tk.END)
            messagebox.showinfo("Success", "Changes committed successfully!")
            self.refresh_status()

    def list_branches(self):
        output = self.run_git(["branch", "-a"])
        if output:
            self.branch_list_display.config(state='normal')
            self.branch_list_display.delete('1.0', tk.END)
            self.branch_list_display.insert(tk.END, output)
            self.branch_list_display.config(state='disabled')

    def switch_branch(self):
        name = self.branch_name_var.get().strip()
        if not name:
            messagebox.showwarning("Input Needed", "Enter a branch name to switch to.")
            return
        if self.run_git(["checkout", name]):
            self.refresh_status()
            messagebox.showinfo("Branch Switch", f"Now on branch: {name}")

    def create_branch(self):
        name = self.branch_name_var.get().strip()
        if not name:
            messagebox.showwarning("Input Needed", "Enter a name for the new branch.")
            return
        if self.run_git(["checkout", "-b", name]):
            self.refresh_status()
            messagebox.showinfo("Branch Created", f"Created and switched to: {name}")

    # --- Versioning Logic ---

    def refresh_version_data(self):
        # 1. Get Local Tag
        local_tag = self.run_git(["describe", "--tags", "--abbrev=0"])
        if local_tag:
            self.lbl_local_tag.config(text=f"Latest Local Tag: {local_tag.strip()}")
        else:
            self.lbl_local_tag.config(text="Latest Local Tag: None found")

        # 2. Get GitHub Release (Async-like feel using UI update)
        try:
            url = f"https://api.github.com/repos/{self.github_user}/{self.github_repo}/releases/latest"
            headers = {"User-Agent": "UCCC-Git-Helper"}
            req = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(req, timeout=5) as response:
                data = json.loads(response.read().decode())
                tag = data.get("tag_name", "Unknown")
                self.lbl_github_ver.config(text=f"Latest on GitHub: {tag}", foreground="blue")
        except Exception as e:
            self.lbl_github_ver.config(text="Latest on GitHub: Error fetching", foreground="red")
            self.log_message(f"GitHub API Error: {str(e)}", "red")

    def create_local_tag(self):
        tag = self.new_tag_var.get().strip()
        msg = self.tag_msg_entry.get().strip()
        
        if not tag:
            messagebox.showwarning("Input Needed", "Please provide a tag name (e.g. v1.0.46)")
            return
            
        args = ["tag", "-a", tag, "-m", msg if msg else f"Release {tag}"]
        if self.run_git(args) is not None:
                messagebox.showinfo("Success", f"Local tag {tag} created.")
                self.update_version_file(tag) # Update version files after tag creation
                self.refresh_version_data()

    def update_source_code_version(self):
        new_version = self.new_tag_var.get().strip()
        if not new_version:
            self.log_message("Error: New tag version is empty. Cannot update source code files.", "red")
            return

        # Define files and their regex patterns for version strings
        version_files = [
            ("CMakeLists.txt", r"(VERSION\s+)(\d+\.\d+\.\d+)", 2), # Group 2 is the version number
            ("install.sh", r"(VERSION=\"v)(\d+\.\d+\.\d+)\")", 2), # Group 2 is the version number
            ("PKGBUILD", r"(pkgver=)(\d+\.\d+\.\d+)", 2), # Group 2 is the version number
            ("config/app_config.json", r"(\"version\":\s*\")(\d+\.\d+\.\d+)(\")", 2) # Group 2 is the version number
        ]

        for file_path, pattern, group_to_replace in version_files:
            full_path = os.path.join(self.repo_path, file_path)
            if not os.path.exists(full_path):
                self.log_message(f"Warning: Version file not found: {file_path}", "yellow")
                continue

            try:
                with open(full_path, "r") as f:
                    content = f.read()
                
                # Use re.sub to replace the version string
                # We need to construct the replacement string carefully to keep other parts of the pattern
                def replacer(match):
                    # Extract all groups and replace only the specified one
                    groups = list(match.groups())
                    groups[group_to_replace - 1] = new_version.lstrip("v") # Remove \'v\' if present for internal files
                    # Reconstruct the string using original parts and updated version
                    return "".join(groups)

                new_content, num_replacements = re.subn(pattern, replacer, content, 1) # Only replace first occurrence

                if num_replacements > 0:
                    with open(full_path, "w") as f:
                        f.write(new_content)
                    self.log_message(f"Updated version in {file_path} to {new_version}", "green")
                else:
                    self.log_message(f"Warning: Version pattern not found in {file_path}. Manual update may be required.", "yellow")

            except Exception as e:
                self.log_message(f"Error updating version in {file_path}: {e}", "red")
    def push_tag_to_remote(self):
        tag = self.new_tag_var.get().strip()
        if not tag:
            # Try to get the latest local tag if entry is empty
            tag_output = self.run_git(["describe", "--tags", "--abbrev=0"])
            if tag_output:
                tag = tag_output.strip()
            else:
                messagebox.showwarning("Input Needed", "Enter a tag to push or create one first.")
                return

        if messagebox.askyesno("Confirm Push", f"Push tag \'{tag}\' to GitHub?"):
            # Ensure local changes are committed before pushing a new tag
            status_output = self.run_git(["status", "--porcelain"])
            if status_output and status_output.strip():
                messagebox.showwarning("Uncommitted Changes", "Please commit or stash your changes before pushing a tag.")
                return

            # Push commits and then the tag
            if self.run_git(["push", "origin", "HEAD"]) is not None:
                if self.run_git(["push", "origin", tag]) is not None:
                    messagebox.showinfo("Success", f"Tag {tag} pushed to GitHub!")
                    self.refresh_version_data()
                else:
                    messagebox.showerror("Error", f"Failed to push tag {tag} to GitHub.")
            else:
                messagebox.showerror("Error", "Failed to push commits to origin before tagging.")


if __name__ == "__main__":
    root = tk.Tk()
    
    # Check if this script is being called by Git/SSH to provide a password
    if len(sys.argv) > 1:
        root.withdraw()
        prompt = sys.argv[1]
        passphrase = simpledialog.askstring("Git Credentials", prompt, show=\'*\')
        if passphrase is not None:
            print(passphrase)
        sys.exit(0)

    # Apply a slightly more modern style
    style = ttk.Style()
    style.theme_use(\'clam\')
    app = GitManagerGUI(root)
    root.mainloop()

    def push_tag_to_remote(self):
        tag = self.new_tag_var.get().strip()
        if not tag:
            # Try to get the latest local tag if entry is empty
            tag_output = self.run_git(["describe", "--tags", "--abbrev=0"])
            if tag_output:
                tag = tag_output.strip()
            else:
                messagebox.showwarning("Input Needed", "Enter a tag to push or create one first.")
                return

        if messagebox.askyesno("Confirm Push", f"Push tag \'{tag}\' to GitHub?"):
            # Ensure local changes are committed before pushing a new tag
            status_output = self.run_git(["status", "--porcelain"])
            if status_output and status_output.strip():
                messagebox.showwarning("Uncommitted Changes", "Please commit or stash your changes before pushing a tag.")
                return

            # Push commits and then the tag
            if self.run_git(["push", "origin", "HEAD"]) is not None:
                if self.run_git(["push", "origin", tag]) is not None:
                    messagebox.showinfo("Success", f"Tag {tag} pushed to GitHub!")
                    self.refresh_version_data()
                else:
                    messagebox.showerror("Error", f"Failed to push tag {tag} to GitHub.")
            else:
                messagebox.showerror("Error", "Failed to push commits to origin before tagging.")


if __name__ == "__main__":
    root = tk.Tk()
    
    # Check if this script is being called by Git/SSH to provide a password
    if len(sys.argv) > 1:
        root.withdraw()
        prompt = sys.argv[1]
        passphrase = simpledialog.askstring("Git Credentials", prompt, show=\'*\')
        if passphrase is not None:
            print(passphrase)
        sys.exit(0)

    # Apply a slightly more modern style
    style = ttk.Style()
    style.theme_use(\'clam\')
    app = GitManagerGUI(root)
    root.mainloop()

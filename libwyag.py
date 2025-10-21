#!/usr/bin/env python3

from datetime import datetime
from fnmatch import fnmatch
from math import ceil
from typing import Optional
import argparse
import configparser
import grp, pwd
import hashlib
import os
import re
import sys
import zlib

argparser = argparse.ArgumentParser(description="Content tracker")

argsubparsers = argparser.add_subparsers(title="Commands", dest="command")
argsubparsers.required = True

def main(argv: list[str] = sys.argv[1:]) -> None:
    '''
    Entry point for the Git-like command-line interface

    Parses command-line args and dispatches execution to the appropriate sbucommand hanlder
    based on the provided command name.

    Params:
        argv : list of str, optional
            The list of command-line args to parse (default: sys.argv[1:])

    Returns:
        None
    '''
    args = argparser.parse_args(argv)
    match args.command:
        case "add"          : cmd_add(args)
        case "cat-file"     : cmd_cat_file(args)
        case "check-ignore" : cmd_check_ignore(args)
        case "checkout"     : cmd_checkout(args)
        case "commit"       : cmd_commit(args)
        case "hash-object"  : cmd_hash_object(args)
        case "init"         : cmd_init(args)
        case "log"          : cmd_log(args)
        case "ls-files"     : cmd_ls_files(args)
        case "ls-tree"      : cmd_ls_tree(args)
        case "rev-parse"    : cmd_rev_parse(args)
        case "rm"           : cmd_rm(args)
        case "show-ref"     : cmd_show_ref(args)
        case "status"       : cmd_status(args)
        case "tag"          : cmd_tag(args)
        case _              : print("Bad command.")

class GitRepository(object):
    """A git repo"""

    worktree = None
    gitdir = None
    conf = None

    def __init__(self, path: str, force: bool=False) -> None:
        '''
        Entry point for git init function

        Params:
            path: str
                The path to the github repo
            force: bool
                flag to disable all checks
        Returns:
            None
        '''

        self.worktree = path
        self.gitdir = os.path.join(path, ".git")

        if not (force or os.path.isdir(self.gitdir)):
            raise Exception(f"Not a Git repository {path}")

        # read configuration file in .git/config
        self.conf = configparser.ConfigParser()
        cf = repo_file(self, "config")

        if cf and os.path.exists(cf):
            self.conf.read([cf])
        elif not force:
            raise Exception("Configuration File missing")

        if not force:
            vers = int(self.conf.get("core", "repositoryformatversion"))
            if vers != 0:
                raise Exception("Unsupported repositoryformatversion: {vers}")

def repo_path(repo: GitRepository, *path: tuple[str, ...]) -> str:
    '''
    Compute path under repo's gitdir.

    Params:
        repo: GitRepository
            This is an object of the class GitRepository used to get the
            attribute .gitdir
        *path: tuple[str, ...]
            This argument makes the function variadic. So it allows multiple
            paths to be passed into the repo_path which will be contained in
            a tuple
    Returns:
        repo_path: str
            String of repo.gitdir + *path
    '''
    return os.path.join(repo.gitdir, *path)

def repo_file(repo: GitRepository, *path: tuple[str, ...], mkdir: bool=False) -> Optional[str]:
    '''
    Compute path under repo's gitdir if dirname(*path) is absent.

    Notes: since *path is variadic the mkdir arg must be passed by name

    Behavior:
        repo_file(r, "refs", "remotes", "HEAD") -> creates
        .git/refs/remotes/origin.

    Params:
        repo: GitRepository
            This is an object of the class GitRepository used to get the
            attribute .gitdir
        *path: tuple[str, ...]
            This argument makes the function variadic. So it allows multiple
            paths to be passed into the repo_path which will be contained in
        mkdir: bool, optional
            flag to create repository with path
    Returns:
        repo_path: Optional[str]
            if dirname(*path) is absent it will create repo_path up to the
            last component, meaning the last component is not added in the
            path
    '''
    if repo_dir(repo, *path[:-1], mkdir=mkdir):
        return repo_path(repo, *path)

def repo_dir(repo: GitRepository, *path: tuple[str, ...], mkdir: bool=False) -> Optional[str]:
    '''
    Compute path under repo's gitdir. Create directory if the directory is
    absent and the mkdir flag is True

    Notes: since *path is variadic the mkdir arg must be passed by name

    Params:
        repo: GitRepository
            This is an object of the class GitRepository used to get the
            attribute .gitdir
        *path: tuple[str, ...]
            This argument makes the function variadic. So it allows multiple
            paths to be passed into the repo_path which will be contained in
        mkdir: bool, optional
    Returns:
        repo_path: Optional[str]
            if dirname(*path) is absent it will create repo_path up to the
            last component, meaning the last component is not added in the
    '''

    path = repo_path(repo, *path)

    if os.path.exists(path):
        if (os.path.isdir(path)):
            return path
        else:
            raise Excpetion(f"Not a directory {path}")

    if mkdir:
        os.makedirs(path)
        return path
    else:
        return None

def repo_create(path: str) -> GitRepository:
    '''
    Creates a new repository at path specificed

    Params:
        path: str
            location where to create new repository

    Return:
        repo: GitRepository
            new repository created at path
    '''

    repo = GitRepository(path, True)

    # ensure path either doesn't exists or is an empty dir
    if os.path.exists(repo.worktree):
        if not os.path.isdir(repo.worktree):
            raise Exception (f"{path} is not a directory!")
        if os.path.exists(repo.gitdir) and os.listdir(repo.gitdir):
            raise Exception (f"{path} is not empty!")
    else:
        os.makedirs(repo.worktree)

    assert repo_dir(repo, "branches", mkdir=True)
    assert repo_dir(repo, "objects", mkdir=True)
    assert repo_dir(repo, "refs", "tags", mkdir=True)
    assert repo_dir(repo, "refs", "heads", mkdir=True)

    # .git/description
    with open(repo_file(repo, "description"), "w") as f:
        f.write("Unnamed repository; edit this file 'description' to name the respository.\n")

    # .git/HEAD
    with open(repo_file(repo, "HEAD"), "w") as f:
        f.write("ref: refs/heads/master\n")

    with open(repo_file(repo, "config"), "w") as f:
        config = repo_default_config()
        config.write(f)

    return repo

def repo_default_config() -> configparser.ConfigParser:
    '''
    Creates default config parser for repository creation

    Params:
        None
    Returns:
        ret: configparser.ConfigParser
            one section core:
            three fields:
                1. repositoryformatversion - version of gitdir format
                2. filemode - tracking of file modes and changes in work tree
                3. bare - indicates if repository has a worktree
    '''
    ret = configparser.ConfigParser()

    ret.add_section("core")
    ret.set("core", "repositoryformatversion", "0")
    ret.set("core", "filemode", "false")
    ret.set("core", "bare", "false")

    return ret

if __name__ == "__main__":
    print("here")




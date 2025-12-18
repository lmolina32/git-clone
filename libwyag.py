#!/usr/bin/env python3

from datetime import datetime
from fnmatch import fnmatch
from math import ceil
from typing import *
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

argsp = argsubparsers.add_parser("init", help="Initialize a new, empty repository.")

argsp.add_argument("path",
                   metavar="directory",
                   nargs="?",
                   default=".",
                   help="Where to create the repository.")

def cmd_init(args: argparse.Namespace) -> None:
    '''
    Initialize a new WYAG repository.

    This function is the command-line entry point for `wyag init`.
    It reads the target directory from the parsed command-line arguments
    and calls `repo_create()` to create a new, empty repository there.

    Params:
        args : argparse.Namespace
            Parsed command-line arguments, expected to contain a `path`
            attribute specifying where to create the repository.

    Example:
    >>> cmd_init(argparse.Namespace(path="."))
    # Creates a new WYAG repository in the current directory.
    '''
    repo_create(args.path)

def repo_find(path: str = ".", required: bool = True) -> Optional[GitRepository]:
    '''
    Recursively search for a Git repository starting from the given path.

    This function walks upward through parent directories until it finds
    a folder containing a `.git` subdirectory, which marks the root of
    a Git repository.

    Params:
        path: str, optional
            The starting directory to search from.
            Defaults to the current directory (".").
        required: bool, optional
            If True, raises an Exception when
            no `.git` directory is found. If False, returns None instead.

    Returns:
        Optional[GitRepository]
            A GitRepository object representing the found repository, or
            None if no repository was found and `required` is False.
    '''

    path = os.path.realpath(path)

    if os.path.isdir(os.path.join(path, ".git")):
        return GitRepository(path)

    # if we haven't returned, recurse in parent, if w
    parent = os.path.realpath(os.path.join(path, ".."))

    # bottom case, the path is root -> os.path.join("/", "..") == "/"
    if parent == path:
        if required:
            raise Exception("Not git directory")
        else:
            return None

    # recursive case
    return repo_find(parent, required)


# files in gits are paths who are determined by their contents

class GitObject(Object):
    """
    Abstract base class for a Git object (e.g., blob, tree, commit, tag).

    This class provides the common interface for serializing and deserializing
    Git objects to and from their raw byte representations. Subclasses must
    implement the `serialize` and `deserialize` methods.
    """

    def __init__(self, data: Optional[bytes] = None) -> None:
        """
        Initializes the GitObject.

        If raw byte data is provided, it immediately deserializes it.
        Otherwise, it calls the `init()` method for default initialization.

        params:
            data: An optional byte string containing the raw object data
                  to deserialize. Defaults to None.
        """
        if data != None:
            self.deserialize(data)
        else:
            self.init()

    def serialize(self, repo):
        '''This function MUST be implemented by subclasses

        It must read the objects\'s contents from self.data, a byte string, and do
        whatever it takes to convert it into a meaningful representation. What exactly
        that means depends on each subclass'''
        raise NotImplementedError("Subclass must implement serialize()")

    def deserialize(self, data: bytes) -> None:
        """
        Deserializes raw byte data into the object's meaningful representation.

        This method MUST be implemented by subclasses. It must parse the
        provided byte string and populate the object's internal attributes
        (e.g., `self.data`, `self.commit_message`, etc.).

        Args:
            data: The raw byte string content of the object to parse.

        Raises:
            NotImplementedError: If the subclass does not implement this method.
        """
        raise NotImplementedError("Subclass must implement deserialize()")

    def init(self) -> None:
        """
        Provides a default initializer for an object created without data.

        This method is called by `__init__` if no `data` is provided.
        Subclasses can override this to set up a default, empty state.
        """
        pass  # Default implementation does nothing.


def object_read(repo: GitRepository, sha: str) -> Optional[GitObject]:
    """
    Read, decompress, and deserialize a Git object from the repository.

    This function locates the object file in the .git/objects/ directory
    based on its SHA-1 hash, reads its compressed content, decompresses
    it, and parses it into the correct GitObject subclass (e.g., GitBlob,
    GitTree, GitCommit, or GitTag).

    Args:
        repo: The GitRepository object to read from.
        sha: The 40-character SHA-1 hash (string) of the object to read.

    Returns:
        An instance of a GitObject subclass populated with the object's data.

    Raises:
        FileNotFoundError: If no object file is found for the given SHA.
        ValueError: If the object header is malformed, the size is incorrect,
                    or the object type is unknown.
        zlib.error: If the object data cannot be decompressed.
    """

    path = repo_file(repo, "objects", sha[0:2], sha[2:])

    if not os.path.isfile(path):
        return None

    with open(path, "rb") as f:
        raw = zlib.decompress(f.read())

        # read object type
        x = raw.find(b' ')
        fmt = raw[0:x]

        # read and validate object size
        y = raw.find(b'\x00', x)
        size = int(raw[x:y].decode("ascii"))
        if size != len(raw)-y-1:
            raise Exception(f"Malformed Object {sha}: bad length")

        # Pick Constructor
        match fmt:
            case b'commit' : c=GitCommit
            case b'tree'   : c=GitTree
            case b'tag'    : c=GitTag
            case b'blob'   : c=GitBlob 
            case _:
                raise Exception(f"Unkown type {fmt.decode("ascii")} for object {sha}")
            
        # call constructor and return object 
        return c(raw[y+1:1])
    
def object_write(obj: GitObject, repo: GitRepository=None) -> str:
    """
    Serialize an object, compute its content-addressed SHA-1 hash, and
    optionally write it to the object store.

    The object is serialized using `obj.serialize()`, prefixed with a header
    of the form:

        "<type> <size>\\0<data>"

    where `<type>` is `obj.fmt` and `<size>` is the length of the serialized
    data in bytes. The SHA-1 hash of this byte sequence is used as the object's
    identifier.

    If a repository is provided, the object is compressed with zlib and written
    to the repository's `objects/` directory using the standard split-hash
    layout (`objects/aa/bb...`). If the object already exists, it is not
    rewritten.

    Args:
        obj: An object with a `fmt` attribute and a `serialize()` method that
             returns its raw byte representation.
        repo: Optional repository in which to store the object. If None, the
              object is not written to disk.

    Returns:
        str: The hexadecimal SHA-1 hash identifying the object.
    """

    # seralize object data
    data = obj.serialize()
    
    result = obj.fmt + b' ' + str(len(data)).encode() + b'\x00' + data

    sha = hashlib.sha1(result).hexdigest()
    
    if repo:
        path = repo_file(repo, "objects", sha[0:2], sha[2:], mkdir=True)
        
        if not os.path.exists(path):
            with open(path, 'wb') as f:
                f.write(zlib.compress(result))
    return sha 

class GitBlob(GitObject):
    fmt=b'blob'
    
    def serialize(self):
        return self.blobdata 
    
    def deserialize(self, data):
        self.blobdata = data

argsp = argsubparsers.add_parser("cat-file",
                                 help="Provide content of repository objects")

argsp.add_argument("type",
                   metavar="type",
                   choices=["blob", "commit", "tag", "tree"],
                   help="Specify the type")
argsp.add_argument("object",
                   metavar="object",
                   help="The Object to display")

def cmd_cat_file(args: Any) -> None:
    """
    Entry point for the `cat-file` command.

    Locates the current repository and prints the contents of the specified
    object to standard output. The object type may be optionally constrained
    via the command-line arguments.

    Args:
        args: Parsed command-line arguments containing:
              - object: The object name or hash to display.
              - type:   Optional object type to enforce.
    """
    repo = repo_find()
    cat_file(repo, args.object, fmt=args.type.encode())
    return 
    
def cat_file(repo: GitRepository, obj: GitObject, fmt: Optional[bytes] = None) -> None:
    """
    Read a repository object and write its raw contents to stdout.

    The object is resolved using `object_find`, read from the object database,
    and its serialized byte representation is written directly to standard
    output without interpretation.

    Args:
        repo: The repository containing the object.
        obj:  The object name or hash to read.
        fmt:  Optional object type (as bytes) to verify against the stored
              object.
    """
    obj = object_read(repo, object_find(repo, obj, fmt=fmt))
    sys.stdout.buffer.write(obj.serialize())
    

def object_find(repo: GitRepository, name: str, fmt: Optional[bytes] = None, follow: bool =True) -> str:
    """
    Resolve an object name to its corresponding object identifier.

    This is a placeholder implementation. For now, it simply returns the
    provided name unchanged. In a complete implementation, this function
    would handle abbreviated hashes, tags, and reference resolution.

    Args:
        repo:   The repository in which to resolve the object.
        name:   The object name or hash.
        fmt:    Optional expected object type.
        follow: Whether to dereference tags or symbolic references.

    Returns:
        The resolved object identifier (currently identical to `name`).
    """
    return name 

argsp = argsubparsers.add_parser("hash-object", help="Compute object ID and optionally creates a blob from a file")

argsp.add_argument("-t", 
                   metavar="type",
                   dest="type",
                   choices=["blob", "commit", "tag", "tree"],
                   default="blob",
                   help="Specify the type")

argsp.add_argument("-w", 
                   dest="write",
                   choices=["blob", "commit", "tag", "tree"],
                   default="blob",
                   help="Specify the type")

argsp.add_argument("path", 
                   help="Read object from <file>")

def cmd_hash_object(args: Any):
    """
    Entry point for the `hash-object` command.

    Reads a file from disk, computes its Git object hash, and optionally writes
    the object to the repository. If the `--write` flag is not provided, the
    object is hashed but not stored.

    Args:
        args: Parsed command-line arguments containing:
              - path:  Path to the file to hash.
              - type:  Object type (e.g., 'blob', 'tree', 'commit', 'tag').
              - write: Boolean flag indicating whether to write the object
                       into the repository.
    """
    if args.write:
        repo = repo_find()
    else:
        repo = None 
        
    with open(args.path, "rb") as fd:
        sha = object_hash(fd, args.type.encode(), repo)
        print(sha)
    
def object_hash(fd: BinaryIO, fmt: bytes, repo: GitRepository) -> str:
    """
    Compute the hash of a Git object from a file descriptor and optionally
    write it to a repository.

    The file contents are read in full, wrapped in the appropriate Git object
    type based on `fmt`, and passed to `object_write()`. If `repo` is None, the
    object is not written and only the hash is returned.

    Args:
        fd:   A binary file-like object opened for reading.
        fmt:  The object type as bytes (e.g., b'blob', b'tree', b'commit',
              b'tag').
        repo: Optional repository in which to store the object.

    Returns:
        str: The hexadecimal SHA-1 hash of the object.
    """
    
    data = fd.read()
    
    match fmt:
        case b'commit' : obj=GitCommit(data)
        case b'tree'   : obj=GitTree(data)
        case b'tag'    : obj=GitTag(data)
        case b'blob'   : obj=GitBlob(data)
        case _: raise Exception(f"Unknown type {fmt}!")
        
    return object_write(obj, repo)

if __name__ == "__main__":
    print("here")




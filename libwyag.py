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

class GitObject(object):
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
        fd:   A binary file-like object opened for reading.j
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

def kvlm_parse(
    raw: bytes,
    start: int = 0,
    dct: Optional[Dict[Optional[bytes], Union[bytes, List[bytes]]]] = None
) -> Dict[Optional[bytes], Union[bytes, List[bytes]]]:
    """
    Parse a KVLM (Key-Value List with Message) formatted byte sequence.

    This function recursively parses a byte string containing key-value
    metadata entries followed by a message body, as used in Git object
    headers (e.g., commit and tag objects).

    Each metadata entry has the form:
        key<space>value\\n

    Values may span multiple continuation lines; continuation lines
    begin with a single space character and are folded into the value
    with the leading space removed.

    Parsing stops at the first blank line (i.e., a newline at the current
    position). The remainder of the input is treated as the message body
    and stored in the dictionary under the key `None`.

    If a key appears multiple times, its values are stored as a list
    of byte strings.

    Args:
        raw (bytes): The raw KVLM-formatted data to parse.
        start (int, optional): Byte offset at which parsing should begin.
            Used internally for recursion. Defaults to 0.
        dct (dict, optional): Dictionary used to accumulate parsed results.
            This should not be provided by callers; it exists to support
            recursive parsing. Defaults to None.

    Returns:
        dict: A dictionary mapping:
            - `bytes` keys to `bytes` values or `List[bytes]` if repeated
            - `None` to the message body (`bytes`)

    Raises:
        AssertionError: If a malformed KVLM structure is encountered
            (e.g., missing expected newline).

    Example:
        >>> raw = b"tree abc123\\nauthor Alice\\n\\nCommit message"
        >>> kvlm_parse(raw)
        {
            b"tree": b"abc123",
            b"author": b"Alice",
            None: b"Commit message"
        }
    """
    if not dct:
        dct = dict()
        
    space = raw.find(b' ', start)
    new_line = raw.find(b'\n', start)
    if (space < 0) or (new_line < space):
        assert new_line == start 
        dct[None] = raw[start+1:]
        return dct
    
    key = raw[start:space]
    
    end = start 
    while True:
        end = raw.find(b'\n', end+1)
        if raw[end+1] != ord(' '): break 
        
    value = raw[space+1:end].replace(b'\n ', b'\n')
    if key in dct:
        if type(dct[key]) == list:
            dct[key].append(value)
        else:
            dct[key] = [dct[key], value]
    else:
        dct[key]=value

    return kvlm_parse(raw, start=end+1, dct=dct)

def kvlm_serialize(
    kvlm: Dict[Optional[bytes], Union[bytes, List[bytes]]]
) -> bytes:
    """
    Serialize a KVLM (Key-Value List with Message) structure into bytes.

    This function converts a dictionary representation of KVLM data
    (as produced by `kvlm_parse`) back into its canonical byte format,
    suitable for storage in Git objects such as commits or tags.

    The input dictionary maps:
        - `bytes` keys to `bytes` values or lists of `bytes`
        - `None` to the message body (`bytes`)

    For each key-value pair:
        - Each value is written as: key<space>value<newline>
        - Multi-line values are serialized using continuation lines,
          where every newline in the value is replaced by "\n "
        - Repeated keys are emitted once per value, in order

    The message body is appended after a blank line.

    Args:
        kvlm (dict): A dictionary mapping:
            - `bytes` → `bytes` or `List[bytes]` for metadata fields
            - `None` → `bytes` for the message body

    Returns:
        bytes: A serialized KVLM byte sequence.

    Example:
        >>> kvlm = {
        ...     b"tree": b"abc123",
        ...     b"parent": [b"p1", b"p2"],
        ...     b"author": b"Alice",
        ...     None: b"Commit message\\n"
        ... }
        >>> kvlm_serialize(kvlm)
        b'tree abc123\\nparent p1\\nparent p2\\nauthor Alice\\n\\nCommit message\\n'
    """
    seralized = b''
    for k in kvlm.keys():
        if k == None: continue # skip msg 
        val = kvlm[k]
        if type(val) != list:
            val = [ val ]
        for v in val:
            seralized += k + b' ' + (v.replace(b'\n', b'\n ')) + b'\n'
        
    seralized += b'\n' + kvlm[None]
    return seralized
   
   

class GitCommit(GitObject):
    fmt=b'commit'
    
    def deserialize(self, data):
        self.kvlm = kvlm_parse(data)
        
    def serialize(self):
        return kvlm_serialize(self.kvlm)
    
    def init(self):
        self.kvlm = dict() 

        
argsp = argsubparsers.add_parser("log", help="Display history of a given commit.")
argsp.add_argument("commit", default="HEAD",
                   nargs="?",
                   help="Commit to start at")

def cmd_log(args: Any) -> None:
    """
    Generate a Graphviz representation of the commit history.

    This function locates the current repository, resolves a starting
    commit (provided via command-line arguments), and prints a Graphviz
    `digraph` description of the commit DAG to standard output.

    The resulting output can be rendered using Graphviz tools (e.g. `dot`)
    to visualize commit history as a directed acyclic graph.

    Args:
        args: Command-line arguments object. Must contain a `commit`
            attribute identifying the starting commit (e.g., a SHA,
            branch name, or tag).

    Returns:
        None
    """
    repo = repo_find()
    
    print("digraph wyaglog{")
    print(" node[shape=rect]")
    log_graphviz(repo, object_find(repo, args.commit), set())
    print("}")

def log_graphviz(repo: Any, sha: str, seen: Set[str]) -> None:
    """
    Recursively emit Graphviz nodes and edges for commits reachable
    from a given commit.

    This function performs a depth-first traversal of the commit DAG,
    starting from the specified commit SHA. Each commit is printed
    exactly once, and edges are emitted from each commit to its parent(s).

    To avoid infinite recursion in the presence of merge commits,
    a set of already-seen commit SHAs is maintained.

    Args:
        repo: Repository object used to read Git objects.
        sha (str): The SHA-1 hash of the commit to process.
        seen (set[str]): A set of commit SHAs already visited.

    Returns:
        None
    """
    if sha in seen:
        return 
    seen.add(sha)
    
    commit = object_read(repo, sha)
    message = commit.kvlm[None].decode("utf8").strip()
    message = message.replace("\\", "\\\\")
    message = message.replace("\"", "\\\"")
    
    if "\n" in message:
        message = message[:message.index("\n")]
        
    print(f" c_{sha} [label=\"{sha[0:7]}: {message}\"]")
    assert commit.fmt==b'commit'
    
    if not b'parent' in commit.kvlm.keys():
        return 
    
    parents = commit.kvlm[b'parent']
    
    if type(parents) != list:
        parents = [ parents ]
        
    for p in parents:
        p = p.decode("ascii")
        print(f" c_{sha} -> c_{p};")
        log_graphviz(repo, p, seen)

if __name__ == "__main__":
    print("here")




/* gitignore.c: */

#include "gitignore.h"
#include "objects.h"
#include "index.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <unistd.h>

static const int tri_none = -1; 

/**
 * ruleset_add - appends an ignore rule to a RuleSet
 *
 * @param rs       Pointer to the RuleSet to modify.
 * @param pattern  The pattern string (may be empty or contain globs).
 * @param exclude  true if the pattern excludes matching paths,
 *                 false if it re-includes them (from '!' prefix).
 **/
void ruleset_add(RuleSet *rs, const char *pattern, bool exclude){
    if (rs->count == rs->capacity){
        rs->capacity = rs->capacity ? rs->capacity * 2 : 8;
        rs->rules = safe_realloc(rs->rules, rs->capacity * sizeof(IgnoreRule)); 
    }
    rs->rules[rs->count].pattern = safe_strdup(pattern);
    rs->rules[rs->count].exclude = exclude;
    rs->count++;
}

/**
 * ruleset_destroy_contents - frees the contents of a RuleSet
 *
 * @param rs  Pointer to the RuleSet whose contents are to be freed.
 **/

void ruleset_destroy_contents(RuleSet *rs){
    for (size_t i = 0; i < rs->count; i++){
        free(rs->rules[i].pattern);
    }
    free(rs->rules);
}

/**
 * parse_line - parses a single line from an ignore file
 *
 * Strips trailing whitespace, skips blank lines and comments,
 * and handles the '!' (re-include) and '\\' (literal) prefixes.
 * On success, allocates a new pattern string and returns true.
 *
 * @param raw_line     The raw line (without trailing newline).
 * @param pattern_out  Pointer to receive the allocated pattern string.
 * @param exclude_out  Pointer to receive the exclude flag
 *                     (true = exclude, false = re-include).
 *
 * @return true if a rule was parsed, false for blank/comment lines.
 **/
static bool parse_line(const char *raw_line, char **pattern_out, bool *exclude_out){
    char *line = safe_strdup(raw_line);
    char *end = line + strlen(line);
    while (end > line && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')){
        *--end = '\0';
    }
    char *start = line;
    while (*start == ' ') start++;

    if (*start == '\0' || *start == '#'){ free(line); return false; }
    
    if (*start == '!'){
        *pattern_out = safe_strdup(start + 1);
        *exclude_out = false;
    } else if (*start == '\\'){
        *pattern_out = safe_strdup(start + 1);
        *exclude_out = true;
    } else {
        *pattern_out = safe_strdup(start);
        *exclude_out = true;
    }

    free(line);
    return true;
}

/**
 * parse_lines_into - parses an array of lines into a RuleSet
 *
 * Iterates over the lines, calls parse_line for each, and adds valid
 * rules to the given RuleSet.
 *
 * @param rs     Pointer to the RuleSet to fill.
 * @param lines  Array of strings (no trailing newlines).
 * @param count  Number of lines in the array.
 **/
static void parse_lines_into(RuleSet *rs, char **lines, size_t count){
    for (size_t i = 0; i < count; i++){
        char *parsed;
        bool excluded;
        if (parse_line(lines[i], &parsed, &excluded)){
            ruleset_add(rs, parsed, excluded);
            free(parsed);
        }
    }
}

/**
 * split_lines - splits a text buffer into an array of lines
 *
 * @param text       Pointer to the text buffer.
 * @param len        Length of the text buffer.
 * @param out_count  Pointer to receive the number of lines.
 *
 * @return A heap‑allocated array of heap‑allocated strings. The
 *         caller must free both the array and each string.
 **/
static char **split_lines(const char *text, size_t len, size_t *out_count){
    size_t cap = 16;
    size_t n = 0;
    size_t i = 0;
    char **lines = safe_calloc(cap * sizeof(char *), 1);

    while (i < len){
        size_t start = i;
        while (i < len && text[i] != '\n'){
            i++;
        }

        size_t line_len = i - start;
        if (n == cap){
            cap *= 2;
            lines = realloc(lines, cap * sizeof(char *));
        }

        lines[n] = safe_calloc(line_len + 1, 1);
        memcpy(lines[n], text + start, line_len);
        lines[n][line_len] = '\0';
        n++;
        if (i < len) i++;
    }

    *out_count = n;
    return lines; 
}

/**
 * free_lines - frees an array of strings produced by split_lines
 *
 * @param lines  The array of strings.
 * @param count  Number of elements in the array.
 **/
static void free_lines(char **lines, size_t count){
    for (size_t i = 0; i < count; i++){
        free(lines[i]);
    }
    free(lines);
}

/**
 * gi_add_absolute - appends a RuleSet to the absolute rules list
 *
 * @param gi  Pointer to the GitIgnore.
 * @param rs  The RuleSet to add.
 **/
void gi_add_absolute(GitIgnore *gi, RuleSet rs){
    if (gi->absolute_count == gi->absolute_capacity){
        gi->absolute_capacity = gi->absolute_capacity ? gi->absolute_capacity * 2 : 8;
        gi->absolute = safe_realloc(gi->absolute, gi->absolute_capacity * sizeof(RuleSet));
    }
    gi->absolute[gi->absolute_count++] = rs;
}

/**
 * gi_add_scoped - appends a scoped RuleSet to the GitIgnore
 *
 * @param gi   Pointer to the GitIgnore.
 * @param dir  The directory path (relative to repo root, "" for root).
 * @param rs   The RuleSet to associate with that directory.
 **/
void gi_add_scoped(GitIgnore *gi, const char *dir, RuleSet rs){
    if (gi->scoped_count == gi->scoped_capacity){
        gi->scoped_capacity = gi->scoped_capacity ? gi->scoped_capacity * 2 : 8;
        gi->scoped = safe_realloc(gi->scoped, gi->scoped_capacity * sizeof(ScopedRuleSet));
    }
    gi->scoped[gi->scoped_count].rules = rs;
    gi->scoped[gi->scoped_count].dir = safe_strdup(dir);
    gi->scoped_count++;
}

/**
 * read_file_as_ruleset - reads a file into an absolute RuleSet
 *
 * Opens the file, reads all lines using getline, parses them, and
 * appends the resulting RuleSet to the GitIgnore's absolute list.
 * If the file cannot be opened, nothing is added.
 *
 * @param path  Path to the ignore file.
 * @param gi    Pointer to the GitIgnore to update.
 **/
static void read_file_as_ruleset(const char *path, GitIgnore *gi){
    FILE *f = safe_fopen(path, "r");

    char *line = NULL; 
    size_t linecap = 0;
    ssize_t linelen; 
    RuleSet rs = {0};

    while ((linelen = getline(&line, &linecap, f)) != -1){
        char *pattern;
        bool exclude;
        if (parse_line(line, &pattern, &exclude)){
            ruleset_add(&rs, pattern, exclude);
            free(pattern);
        }
    }
    free(line);
    fclose(f);
    gi_add_absolute(gi, rs);
}

/**
 * path_dirname - returns the directory portion of a path
 *
 * @param path  The full path (relative, no trailing slash).
 *
 * @return A newly allocated string containing the directory part.
 **/
static char *path_dirname(const char *path){
    const char *slash = strrchr(path, '/');
    if (!slash) return safe_strdup("");
    size_t len = (size_t)(slash - path);
    char *out = safe_calloc(len + 1, 1);
    memcpy(out, path, len);
    out[len] = '\0';
    return out;
}

/**
 * gitignore_read - collects all ignore rules for a repository
 *
 * Reads repository‑local exclude file (.git/info/exclude), global
 * ignore file (~/.config/git/ignore or $XDG_CONFIG_HOME/git/ignore),
 * and all .gitignore files stored in the index. Rules are stored as
 * absolute (global) and scoped (per‑directory) sets.
 *
 * @param repo  Pointer to the Repository.
 *
 * @return A newly allocated GitIgnore structure, or NULL on allocation failure.
 *         Must be freed with gitignore_destroy().
 **/
GitIgnore *gitignore_read(Repository *repo){
    GitIgnore *gi = safe_calloc(sizeof(GitIgnore), 1);


    char *exclude_path = repo_file(repo, false, "info/exclude", NULL);
    if (exclude_path && file_exists(exclude_path)){
        read_file_as_ruleset(exclude_path, gi);
    }
    free(exclude_path);

    const char *xdg = getenv("XDG_CONFIG_HOME");
    char *global_path;
    if (xdg){
        global_path = path_join(xdg, "git/ignore", NULL);
    } else {
        const char *home = getenv("HOME");
        global_path = home ? path_join(home, ".config/git/ignore", NULL) : NULL;
    }

    if (global_path && file_exists(global_path)){
        read_file_as_ruleset(global_path, gi);
    }
    free(global_path);

    GitIndex *index = index_read(repo);
    for (size_t i = 0; i < index->count; i++){
        const char *name = index->entries[i].name;
        size_t nlen = strlen(name);
        bool is_gitignore = streq(name, ".gitignore") ||
            (nlen > 11 && streq(name + nlen - 11, "/.gitignore"));
        if (!is_gitignore) continue;

        Object *obj = object_read(repo, index->entries[i].sha);
        if (!obj || obj->type != GIT_BLOB){ object_destroy(obj); continue; }
        
        size_t line_count;
        char **lines = split_lines(obj->data, obj->size, &line_count);
        RuleSet rs = {0};
        parse_lines_into(&rs, lines, line_count);
        free_lines(lines, line_count);

        char *dir = path_dirname(name);
        gi_add_scoped(gi, dir, rs);
        free(dir);

        object_destroy(obj);
    }

    index_destroy(index);
    return gi;
}

/**
 * gitignore_destroy - frees a GitIgnore and all its rules
 *
 * @param gi  Pointer to the GitIgnore to free.
 **/
void gitignore_destroy(GitIgnore *gi){
    if (!gi) return; 

    for (size_t i = 0; i < gi->absolute_count; i++){
        ruleset_destroy_contents(&gi->absolute[i]);
    }
    free(gi->absolute);

    for (size_t i = 0; i < gi->scoped_count; i++){
        ruleset_destroy_contents(&gi->scoped[i].rules);
        free(gi->scoped[i].dir);
    }
    free(gi->scoped);
    free(gi);
}

/**
 * check_ruleset - checks a path against a single RuleSet
 *
 * @param rs    Pointer to the RuleSet.
 * @param path  The path to test (relative to repo root).
 *
 * @return 1 if the path should be ignored, 0 if re‑included,
 *         -1 if no rule matched.
 **/
static int check_ruleset(RuleSet *rs, const char *path){
    int result = tri_none;
    for (size_t i = 0; i < rs->count; i++){
        if (fnmatch(rs->rules[i].pattern, path, 0) == 0){
            result = rs->rules[i].exclude ? 1 : 0;
        }
    }
    return result;
}

/**
 * check_scoped - checks a path against all scoped RuleSets
 *
 * Starting from the path's own directory, moves upward toward the
 * repository root, testing each directory's .gitignore rules. The
 * first directory with at least one matching rule determines the
 * result; more general rules are not consulted.
 *
 * @param gi    Pointer to the GitIgnore.
 * @param path  The path to test.
 *
 * @return 1 if ignored, 0 if re‑included, -1 if no scoped rule matched.
 **/
static int check_scoped(GitIgnore *gi, const char *path){
    char *parent = path_dirname(path);
    for (;;){
        for (size_t i = 0; i < gi->scoped_count; i++){
            if (streq(gi->scoped[i].dir, parent)){
                int result = check_ruleset(&gi->scoped[i].rules, path);
                if (result != tri_none){ free(parent); return result; }
            }
        }
        if (streq(parent, "")){ break; }
        char *next = path_dirname(parent);
        free(parent);
        parent = next;
    }
    free(parent);
    return tri_none;
}

/**
 * check_absolute - checks a path against all absolute RuleSets
 *
 * Tests each absolute RuleSet in order; the first one with a match
 * determines the result.
 *
 * @param gi    Pointer to the GitIgnore.
 * @param path  The path to test.
 *
 * @return 1 if ignored, 0 if re‑included, -1 if no absolute rule matched.
 **/

static int check_absolute(GitIgnore *gi, const char *path){
    for (size_t i = 0; i < gi->absolute_count; i++){
        int result = check_ruleset(&gi->absolute[i], path);
        if (result != tri_none){ return result; }
    }
    return tri_none; 
}

/**
 * check_ignore - determines whether a path should be ignored
 *
 * First tries scoped rules (from .gitignore files in the index),
 * then absolute rules (info/exclude and global ignore). If any rule
 * matches, the result is returned; otherwise the path is not ignored.
 *
 * @param gi    Pointer to the GitIgnore.
 * @param path  The path to test (relative to repo root).
 *
 * @return true if the path is ignored, false otherwise.
 **/
bool check_ignore(GitIgnore *gi, const char *path){
    int result = check_scoped(gi, path);
    if (result != tri_none) return result == 1;

    result = check_absolute(gi, path);
    return result == 1;
}
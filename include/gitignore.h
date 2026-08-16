/* gitignore.h */

#ifndef GITIGNORE_H
#define GITIGNORE_H

#include "repository.h"

/* Structures */

typedef struct {
    char *pattern;
    bool  exclude;
} IgnoreRule;

typedef struct {
    IgnoreRule *rules;
    size_t      count;
    size_t      capacity;
} RuleSet; 

typedef struct {
    char    *dir;
    RuleSet  rules;
} ScopedRuleSet;

typedef struct {
    RuleSet        *absolute;
    size_t         absolute_count;
    size_t         absolute_capacity;
    ScopedRuleSet *scoped;
    size_t         scoped_count;
    size_t         scoped_capacity;
} GitIgnore;

/* Functions */

GitIgnore *gitignore_read(Repository *repo);
void       gitignore_destroy(GitIgnore *gi);
bool       check_ignore(GitIgnore *gi, const char *path);
void       ruleset_add(RuleSet *rs, const char *pattern, bool exclude);
void       ruleset_destroy_contents(RuleSet *rs);
void       gi_add_scoped(GitIgnore *gi, const char *dir, RuleSet rs);
void       gi_add_absolute(GitIgnore *gi, RuleSet rs);


#endif 
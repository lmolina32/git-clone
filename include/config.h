/* config.h */

#ifndef CONFIG_H
#define CONFIG_H

/* structures */

typedef struct {
    char name[256];
    char email[256];
    bool found_name;
    bool found_email;
} GitConfigUser;

/* Functions */

char *gitconfig_user_get();

#endif 
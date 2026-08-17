/* config.c: parse Git configuration files using inih */

#include "config.h"
#include "ini.h"
#include "utils.h"

#include <string.h>

/**
 * gitconfig_handler - Callback for ini_parse to extract user.name and user.email.
 *
 * When the parser encounters a key under the "user" section, this function
 * copies the value into the provided GitConfigUser structure.
 *
 * @param user    Pointer to a GitConfigUser struct.
 * @param section Current section name (e.g., "user").
 * @param key     Key name (e.g., "name").
 * @param value   Value string.
 *
 * @return Always returns 1 (non‑zero) to continue parsing.
 **/
static int gitconfig_handler(void *user, const char *section, const char *key, const char *value){
    GitConfigUser *u = (GitConfigUser *)user;
    if (streq(section, "user")){
        if (streq(key, "name")){
            strncpy(u->name, value, sizeof(u->name) - 1);
            u->name[sizeof(u->name) -1] = '\0';
            u->found_name = true;
        }
        if (streq(key, "email")){
            strncpy(u->email, value, sizeof(u->email) -1);
            u->email[sizeof(u->email) - 1] = '\0';
            u->found_email = true;
        }
    }
    return 1;
}

/**
 * gitconfig_user_get - Retrieve the user's name and email from ~/.gitconfig.
 *
 * Searches both XDG_CONFIG_HOME/git/config and ~/.gitconfig. On success,
 * returns a string like "Name <email>". The caller is responsible for freeing
 * the returned pointer.
 *
 * @return Newly allocated string, or NULL if not found or no valid entry.
 */
char *gitconfig_user_get(){
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    char *candidates[2] = { NULL, NULL };
    if (xdg) candidates[0] = path_join(xdg, "git/config", NULL);
    else if (home) candidates[0] = path_join(home, ".config/git/config", NULL);
    if (home) candidates[1] = path_join(home, ".gitconfig", NULL);

    GitConfigUser u = {0};
    for (int i = 0; i < 2; i++){
        if (candidates[i] && file_exists(candidates[i])){
            ini_parse(candidates[i], gitconfig_handler, &u);
        }
        free(candidates[i]);
    }

    if (!u.found_name || !u.found_email) return NULL;

    char *result = safe_calloc(strlen(u.name) + strlen(u.email) + 4, 1);
    sprintf(result, "%s <%s>", u.name, u.email);
    return result;
}
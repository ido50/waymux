#include <stdio.h>
#include "profile.h"

int main() {
    struct profile *p = profile_load("test-profile");
    if (!p) {
        fprintf(stderr, "Failed to load profile\n");
        return 1;
    }

    printf("Loaded profile: %s\n", p->name);
    printf("Working dir: %s\n", p->working_dir ? p->working_dir : "(none)");
    printf("Proxy command: %s\n", p->proxy_command ? p->proxy_command : "(none)");
    printf("Environment variables: %d\n", p->env_count);
    printf("Tabs: %d\n", p->tab_count);

    for (int i = 0; i < p->tab_count; i++) {
        printf("  Tab %d: %s (title: %s)\n", i, p->tabs[i].command,
               p->tabs[i].title ? p->tabs[i].title : "(none)");
    }

    profile_free(p);
    return 0;
}

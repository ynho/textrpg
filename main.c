/* text-based rpg */

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#define MAX_WORD_LENGTH 40
/* the +20 is to hold "the", "of" and such */
#define MAX_NAME_LENGTH (5 * MAX_WORD_LENGTH + 20)
#define MAX_NEIGHBORS 4
#define MAX_WORDS 10

enum alignment {
    FRIENDLY = 0, NEUTRAL, ENEMY
};

struct quest {
    int code[2][MAX_WORDS];
    int open;
    int bounty;
    int show_bounty;
};

struct combat {
    unsigned int health_pool;
    int health;
    int action_pts_pool;
    int action_pts;
    int action_pts_gain;
    int damage;
};

struct creature {
    int code[2][MAX_WORDS];

    /* various retarded attributes */
    struct combat fight;
    enum alignment align;
    struct quest quest;
};

struct node {
    int code[2][MAX_WORDS];
    unsigned int neighbors[MAX_NEIGHBORS];
    unsigned int n_neighbors;
    int generated;

    struct creature *creature;    
};

struct player {
    struct combat fight;
    char name[MAX_NAME_LENGTH];
    unsigned int node;
    int goldz;
    /* TODO: inventory, equipped items, etc. */
};

struct world {
    /* nodes */
    unsigned int n_nodes, allocated_nodes;
    struct node *nodes;
    unsigned int n_not_generated;
};


static void init_code (int c[2][MAX_WORDS])
{
    unsigned int i;
    for (i = 0; i < MAX_WORDS; i++) {
        c[0][i] = 0;
        c[1][i] = 0;
    }
}

static void copy_code (int a[2][MAX_WORDS], int b[2][MAX_WORDS])
{
    unsigned int i, j;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < MAX_WORDS; j++)
            a[i][j] = b[i][j];
    }
}

static void init_quest (struct quest *q)
{
    init_code (q->code);
    q->open = 0;
    q->bounty = 0;
    q->show_bounty = 0;
}

static void init_combat (struct combat *c)
{
    c->health_pool = 100;
    c->health = c->health_pool;
    c->action_pts_pool = 100;
    c->action_pts = 100;
    c->action_pts_gain = 50;
    c->damage = 40;             /* whatever */
}

static void init_creature (struct creature *c)
{
    unsigned int i;
    init_code (c->code);
    init_combat (&c->fight);
    c->align = NEUTRAL;
    init_quest (&c->quest);
}
static void clear_creature (struct creature *c)
{
    (void)c;
}

static void init_player (struct player *p)
{
    init_combat (&p->fight);
    memset (p->name, 0, MAX_NAME_LENGTH);
    strcpy (p->name, "Jean-Claude"); /* deal with it. */
    p->node = 0;
    p->goldz = 0;
}
static void clear_player (struct player *p)
{
    (void)p;
}

static void init_node (struct node *n)
{
    unsigned int i;
    init_code (n->code);
    for (i = 0; i < MAX_NEIGHBORS; i++)
        n->neighbors[i] = 0;
    n->n_neighbors = 0;
    n->generated = 0;
    n->creature = NULL;
}
static void clear_node (struct node *n)
{
    if (n->creature) {
        clear_creature (n->creature);
        free (n->creature);
    }
}

static void init_world (struct world *w)
{
    w->n_nodes = 0;
    w->allocated_nodes = 0;
    w->nodes = NULL;
    w->n_not_generated = 0;
}
static void clear_world (struct world *w)
{
    unsigned int i;
    for (i = 0; i < w->n_nodes; i++)
        clear_node (&w->nodes[i]);
    free (w->nodes);
}


static void* array_realloc (void *old, size_t old_size, size_t new_size)
{
    void *new = NULL;

    new = malloc (new_size);
    if (new && old) {
        memcpy (new, old, old_size);
        free (old);
    }
    return new;
}

static size_t array_enlarge (void **old, size_t old_size)
{
    if (old_size == 0) {
        *old = array_realloc (*old, old_size, 1);
        return 1;
    } else {
        *old = array_realloc (*old, old_size, 2 * old_size);
        return 2 * old_size;
    }
}

static int max (int a, int b)
{
    return a > b ? a : b;
}
static int min (int a, int b)
{
    return a < b ? a : b;
}

static int random_range (int a, int b)
{
    float r = (float)rand () / (float)RAND_MAX;
    r *= (float)(b - a);
    r += (float)a;
    return (int)(r + 0.5);
}


/* number of levels of rarity */
#define NUM_RARES 3

struct dictionary {
    struct {
        unsigned int n_words;
        char **words;
    } data[NUM_RARES][MAX_WORDS];

    unsigned int sentence_length;
    int probabilities[MAX_WORDS];
    int rarity[NUM_RARES];
    char *articles[MAX_WORDS];
};

static void init_dictionary (struct dictionary *dic)
{
    unsigned int i, j;
    for (i = 0; i < NUM_RARES; i++) {
        dic->rarity[i] = 0;
        for (j = 0; j < MAX_WORDS; j++) {
            dic->data[i][j].n_words = 0;
            dic->data[i][j].words = NULL;
        }
    }

    dic->sentence_length = 0;
    for (i = 0; i < MAX_WORDS; i++) {
        dic->probabilities[i] = 0;
        dic->articles[i] = NULL;
    }
}

/* be careful about using this function :DD:D */
static void copy_dictionary (struct dictionary *a, struct dictionary *b)
{
    unsigned int i, j;
    for (i = 0; i < NUM_RARES; i++) {
        a->rarity[i] = b->rarity[i];
        for (j = 0; j < MAX_WORDS; j++) {
            a->data[i][j].n_words = b->data[i][j].n_words;
            a->data[i][j].words = b->data[i][j].words;
        }
    }

    a->sentence_length = b->sentence_length;
    for (i = 0; i < MAX_WORDS; i++) {
        a->probabilities[i] = b->probabilities[i];
        a->articles[i] = b->articles[i];
    }
}

static void generate_code0 (int code[2][MAX_WORDS], struct dictionary *dic)
{
    unsigned int i;

    for (i = 0; i < dic->sentence_length; i++)
        code[0][i] = (random_range (1, 100) <= dic->probabilities[i]) ? 1 : 0;

    /* TODO: special cases need to be handled by the dictionary */
    {
        /* special case */
        if (!code[0][0])
            code[0][1] = 0;
    }

    /* select the rarity of the words that are gonna be picked */
    for (i = 0; i < dic->sentence_length; i++) {
        if (code[0][i]) {
            int j, r = random_range (1, 100);
            for (j = 0; j < NUM_RARES; j++) {
                if (r <= dic->rarity[j]) {
                    code[0][i] = NUM_RARES - j;
                    break;
                }
            }
        }
    }
}

static void generate_code1 (int code[2][MAX_WORDS], struct dictionary *dic)
{
    unsigned int i;
    /* select the words */
    for (i = 0; i < dic->sentence_length; i++) {
        if (code[0][i])
            code[1][i] = random_range (0, dic->data[code[0][i] - 1][i].n_words - 1);
    }
}

static void generate_code (int code[2][MAX_WORDS], struct dictionary *dic)
{
    generate_code0 (code, dic);
    generate_code1 (code, dic);
}


static void get_name (char *name, int code[2][MAX_WORDS], struct dictionary *dic)
{
    unsigned int i;

    strcpy (name, dic->articles[0]);
    for (i = 0; i < dic->sentence_length; i++) {
        if (code[0][i]) {
            strcat (name, dic->articles[i + 1]);
            strcat (name, dic->data[code[0][i] - 1][i].words[code[1][i]]);
        }
    }
}

static void generate_node (struct world *w, unsigned int n, struct dictionary *dic)
{
    init_node (&w->nodes[n]);
    generate_code (w->nodes[n].code, dic);
    w->n_not_generated++;
    w->n_nodes++;
}

static void connect_nodes (struct world *w, unsigned int a_, unsigned int b_)
{
    struct node *a, *b;

    if (a_ == b_)
        return;

    a = &w->nodes[a_];
    b = &w->nodes[b_];

    if (a->n_neighbors >= MAX_NEIGHBORS ||
        b->n_neighbors >= MAX_NEIGHBORS)
        return;

    a->neighbors[a->n_neighbors] = b_;
    b->neighbors[b->n_neighbors] = a_;
    a->n_neighbors++;
    b->n_neighbors++;
}

static void generate_neighbors (struct world *w, unsigned int node,
                                struct dictionary *dic)
{
    unsigned int start, end, i;
    struct node *n = &w->nodes[node];
    int range;

    if (n->n_neighbors >= MAX_NEIGHBORS || n->generated) {
        fprintf (stderr, "error: trying to re-generate a node\n");
        return;
    }

    /* the closer we are to a finite world, the more likely we are
       to create new nodes */
    range = 10;
    if (w->n_not_generated <= range)
        range -= (range - w->n_not_generated + 1);

    /* connect the node to some already existing nodes */
    start = max (0,          (int)node - range);
    end   = min (w->n_nodes, (int)node + range);

    for (i = start; i < end; i++) {
        if (i != node && !w->nodes[i].generated && n->n_neighbors < MAX_NEIGHBORS) {
            if (random_range (0, 5) < 1)
                connect_nodes (w, node, i);
        }
    }

    /* create new nodes for the remaining slots */
    while ((w->n_nodes + MAX_NEIGHBORS) * sizeof (struct node) >= w->allocated_nodes)
        w->allocated_nodes = array_enlarge (&w->nodes, w->allocated_nodes);
    n = &w->nodes[node];
    end = random_range (max (n->n_neighbors, 1), MAX_NEIGHBORS);
    if (w->n_not_generated == 1)
        end = max (end, n->n_neighbors + 1);
    for (i = n->n_neighbors; i < end; i++) {
        generate_node (w, w->n_nodes, dic);
        connect_nodes (w, w->n_nodes - 1, node);
    }

    n->generated = 1;
    w->n_not_generated--;
}


static void generate_creature (struct world *w, unsigned int node,
                               struct dictionary *dic)
{
    struct node *n = &w->nodes[node];

    if (n->creature) {
        fprintf (stderr, "error: trying to re-generate a creature\n");
        return;
    }

    n->creature = malloc (sizeof *n->creature);
    init_creature (n->creature);
    generate_code (n->creature->code, dic);
}

/* ----- Places ----- */

/* form:
   The [<1> [and <2>]] <3> [of <4>] [where <5>]
   1 and 2 are the adjectives, 3 the noun, 4 some random word and 5
   some random sentence. Parts between brackets are optional.
*/

/* 

file format:

length
prob1 prob2 prob3 ... prob<length>
article1
article2
...
article<length>
rarity position word1
rarity position word2
...
rarity position word<k>

*/

static void buffer_clear (char *ptr)
{
    while (*ptr && *ptr != '\n')
        ptr++;
    *ptr = 0;
}

static int read_dictionary_from_file (FILE *fp, struct dictionary *dic)
{
    const char *msg = "no error";
    char buffer[1024] = {0};          /* TODO: let's hope this is big enough */
    char *end = NULL, *end2 = NULL;
    long a, b, position;
    unsigned int i, j, k;
    int counter[NUM_RARES][MAX_WORDS];

    /* sentence length */
    fgets (buffer, sizeof buffer, fp);
    a = strtol (buffer, &end, 10);
    if (buffer == end || a < 0 || a > MAX_WORDS) {
        msg = "bad sentence length";
        goto fail;
    }
    dic->sentence_length = (unsigned int) a;

    /* probabilities */
    i = 0;
    fgets (buffer, sizeof buffer, fp);
    end = buffer;
    while (*end != '\n') {
        a = strtol (end, &end2, 10);
        if (a == 0) {
            msg = "bad probability";
            goto fail;
        }
        end = end2;
        dic->probabilities[i] = (int) a;
        i++;
    }

    /* rarities */
    i = 0;
    fgets (buffer, sizeof buffer, fp);
    end = buffer;
    while (*end != '\n') {
        a = strtol (end, &end2, 10);
        if (a == 0) {
            msg = "bad rarity";
            goto fail;
        }
        end = end2;
        dic->rarity[i] = (int) a;
        i++;
    }

    /* articles */
    /* +1: first article always used */
    for (i = 0; i < dic->sentence_length + 1; i++) {
        fgets (buffer, sizeof buffer, fp);
        buffer_clear (buffer);
        dic->articles[i] = malloc (strlen (buffer) + 1);
        strcpy (dic->articles[i], buffer);
    }

    /* word counting */
    position = ftell (fp);
    while ((end2 = fgets (buffer, sizeof buffer, fp))) {
        /* blank lines are ok */
        if (*buffer != '\n') {
            a = strtol (buffer, &end, 10);
            if (buffer == end) {
                msg = "bad rarity number";
                goto fail;
            }
            b = strtol (end, &end2, 10);
            if (end == end2) {
                msg = "bad position number";
                goto fail;
            }
            if (b >= dic->sentence_length) {
                msg = "position number too big";
                goto fail;
            }
            dic->data[a][b].n_words++;
        }
    }

    /* memory allocation */
    for (i = 0; i < NUM_RARES; i++) {
        for (j = 0; j < MAX_WORDS; j++) {
            counter[i][j] = 0;  /* sneaky local memory initialization */
            if (dic->data[i][j].n_words) {
                dic->data[i][j].words = malloc (dic->data[i][j].n_words *
                                                sizeof *dic->data[i][j].words);
                for (k = 0; k < dic->data[i][j].n_words; k++)
                    dic->data[i][j].words[k] = NULL;
            }
        }
    }
    

    /* reading */
    fseek (fp, position, SEEK_SET);
    while ((end2 = fgets (buffer, sizeof buffer, fp))) {
        /* blank lines are still ok */
        if (*buffer != '\n') {
            a = strtol (buffer, &end, 10);
            b = strtol (end, &end2, 10);
            while (isspace (*end2))
                end2++;
            dic->data[a][b].words[counter[a][b]] = malloc (strlen (end2) + 1);
            buffer_clear (end2);
            strcpy (dic->data[a][b].words[counter[a][b]], end2);
            counter[a][b]++;
        }
    }

    return 0;
fail:
    fprintf (stderr, "wrong format: %s\n", msg);
    return -1;
}

static int read_dictionary (const char *fname, struct dictionary *dic)
{
    FILE *fp = NULL;
    int err = 0;
    if (!(fp = fopen (fname, "r"))) {
        perror (fname);
        return -1;
    }
    err = read_dictionary_from_file (fp, dic);
    fclose (fp);
    return err;
}

static int write_dictionary_to_file (FILE *fp, struct dictionary *dic)
{
    unsigned int i, j;

    fprintf (fp, "%d\n", dic->sentence_length);
    for (i = 0; i < dic->sentence_length; i++)
        fprintf (fp, "%s%d", (i == 0 ? "" : " "), dic->probabilities[i]);
    fprintf (fp, "\n");
    for (i = 0; i < dic->sentence_length; i++)
        fprintf (fp, "%s\n", dic->articles[i]);
    for (i = 0; i < NUM_RARES; i++) {
        for (j = 0; j < dic->sentence_length; j++) {
            unsigned int k;
            for (k = 0; k < dic->data[i][j].n_words; k++)
                fprintf (fp, "%d %d %s\n", i, j, dic->data[i][j].words[k]);
        }
    }

    fclose (fp);
    return 0;

}

static int write_dictionary (const char *fname, struct dictionary *dic)
{
    FILE *fp = NULL;
    if (!(fp = fopen (fname, "w"))) {
        perror (fname);
        return -1;
    }

}


static float compute_rarity (int code[2][MAX_WORDS], struct dictionary *dic)
{
    float r = 1.0;
    unsigned int i;

    for (i = 0; i < dic->sentence_length; i++) {
        if (!code[0][i]);
            /* r *= 1.0 - dic->probabilities[i] / 100.0; */
        else {
            r *= dic->probabilities[i] / 100.0;
            r *= dic->rarity[NUM_RARES - code[0][i]] / 100.0;
            r /= dic->data[NUM_RARES - code[0][i]][i].n_words;
        }
    }

    return r;
}

static float compute_scaled_rarity (int code[2][MAX_WORDS], struct dictionary *dic)
{
    float r = 1.0;
    int most[2][MAX_WORDS];
    unsigned int i;

    for (i = 0; i < dic->sentence_length; i++)
        most[0][i] = most[1][i] = 0;

    /* get most probable code */
    for (i = 0; i < dic->sentence_length; i++) {
        /* if (dic->probabilities[i] > 50) */
        if (dic->probabilities[i] == 100)
            most[0][i] = 1;
    }
    r = compute_rarity (most, dic);

    return compute_rarity (code, dic) / r;
}

/* is a included in b? */
static int is_code_included (int a[2][MAX_WORDS], int b[2][MAX_WORDS])
{
    unsigned int i;

    for (i = 0; i < MAX_WORDS; i++) {
        if (b[0][i]) {
            if (a[0][i] != b[0][i] || a[1][i] != b[1][i])
                return 0;
        }
    }

    return 1;
}

static void generate_code_range (int code[2][MAX_WORDS], struct dictionary *dic,
                                 float a, float b)
{
    float r;
    unsigned int tries = 0;
    const unsigned int max_tries = 5000; /* TODO: we might wanna increase that */

    do {
        generate_code0 (code, dic);
        r = compute_scaled_rarity (code, dic);
        tries++;
    } while ((r > b || r < a) && tries < max_tries);

    if (tries == max_tries)
        fprintf (stderr, "duh your rarity is not achievable\n");
    else
        generate_code1 (code, dic);
}

static void generate_code_approx (int code[2][MAX_WORDS], struct dictionary *dic,
                                  float target)
{
    struct dictionary local;
    unsigned int i;
    int c[2][MAX_WORDS];
    float margin = 1.0;
    const unsigned int max_tries = 3000; /* Xd */

    init_dictionary (&local);
    copy_dictionary (&local, dic);
    /* eliminate the rarity factor to have an evenly distributed output */
    for (i = 0; i < NUM_RARES; i++)
        local.rarity[i] = 100 / NUM_RARES;
    local.rarity[0] += 100 % NUM_RARES; /* Xd */

    for (i = 0; i < max_tries; i++) {
        float a;
        generate_code (c, &local);
        a = fabs (target - compute_scaled_rarity (c, dic));
        if (a < margin) {
            copy_code (code, c);
            margin = a;
        }
    }
}

enum combat_command {
    ATTACK = 0, DRINK, LAST_COMBAT_COMMAND
};

static const int command_cost[LAST_COMBAT_COMMAND] = {40, 10};

/* a performs cmd on b */
static void issue_combat_command (enum combat_command cmd, struct combat *a,
                                  struct combat *b)
{
    if (a->action_pts < command_cost[cmd])
        return;                 /* ahem. */

    switch (cmd) {
    case ATTACK:
        b->health -= a->damage;
        break;
    case DRINK:
        a->health += 20; /* gives a fixed amount of HP */
        if (a->health > a->health_pool)
            a->health = a->health_pool;
        break;
    default:;
    }
    a->action_pts -= command_cost[cmd];
}

static int increase_action_pts (struct combat *c)
{
    c->action_pts += c->action_pts_gain;
    if (c->action_pts > c->action_pts_pool)
        c->action_pts = c->action_pts_pool;
}

/* gives AI computed command for a regarding b */
static enum combat_command combat_ai (struct combat *a, struct combat *b)
{
    return ATTACK;                   /* smart IA. */
}

static int is_dead (struct combat *c)
{
    return c->health <= 0;
}

#define MAX_INPUT_SIZE 32

static int fight_loop (struct player *p, struct combat *c)
{
    char input[MAX_INPUT_SIZE];

    printf ("Commands:\n1 - Attack\n2 - Drink health potion!\n3 - End your turn.\n");
    /* init action points */
    p->fight.action_pts = p->fight.action_pts_pool;
    c->action_pts = c->action_pts_pool;

    while (1) {
        long number;

        printf ("      You ; Opponent\n"
                "HP -- %d ; %d\nAction points -- %d ; %d\n",
                p->fight.health, c->health, p->fight.action_pts, c->action_pts);

        memset (input, 0, MAX_INPUT_SIZE);
        fgets (input, MAX_INPUT_SIZE, stdin);

        number = strtol (input, NULL, 10);
        if (number < 1 || number > LAST_COMBAT_COMMAND + 1) {
            printf ("Wrong input. Try again: ");
            fflush (stdout);
            continue;
        }

        if (number < LAST_COMBAT_COMMAND + 1) {
            if (p->fight.action_pts >= command_cost[number - 1])
                issue_combat_command (number - 1, &p->fight, c);
            else
                printf ("Not enough action points to perform this action!\n");
        } else {
            /* AI mode */
            enum combat_command cmd;
            cmd = combat_ai (c, &p->fight);
            while (c->action_pts >= command_cost[cmd]) {
                issue_combat_command (cmd, c, &p->fight);
                if (is_dead (&p->fight))
                    break;
                cmd = combat_ai (c, &p->fight);
            }
            /* end of turn, restore dem action points */
            increase_action_pts (&p->fight);
            increase_action_pts (c);
        }

        /* check if someone is dead */
        if (is_dead (c)) {
            printf ("Yay you have beaten the beast!\n");
            return 1;
        } else if (is_dead (&p->fight)) {
            printf ("You died.\n");
            return 0;
        }
    }
}

#define NUM_MAX_PLAYERS 32

struct game {
    struct world w;
    struct player p; /* offline single-player backward compatibility */
    struct dictionary dic_places, dic_creatures;
    unsigned int n_players;
    struct player players[NUM_MAX_PLAYERS];

    FILE *in, *out;             /* IRC mode */
};

static void init_game (struct game *g)
{
    unsigned int i;
    init_world (&g->w);
    init_player (&g->p);
    init_dictionary (&g->dic_places);
    init_dictionary (&g->dic_creatures);
    g->n_players = 0;
    for (i = 0; i < NUM_MAX_PLAYERS; i++)
        init_player (&g->players[i]);
    g->in = g->out = NULL;
}

#define CREATURE_CHANCE 400

static void grow_world (struct game *g, unsigned int node)
{
    struct node *n;

    n = &g->w.nodes[node];
    if (!n->generated) {
        generate_neighbors (&g->w, node, &g->dic_places);
        n = &g->w.nodes[node]; /* generate_neighbors() might reallocate */
        if (random_range (1, 100) <= CREATURE_CHANCE) {
            generate_creature (&g->w, node, &g->dic_creatures);
            n->creature->align = random_range (0, ENEMY);
            n->creature->align = ENEMY; /* TODO: tmp. */

            if (/* some random &&*/ n->creature->align == FRIENDLY
                || n->creature->align == NEUTRAL) {
                /* pick a rarity number */
                float r = compute_scaled_rarity (n->creature->code, &g->dic_creatures);
                /* generate SOMETHING */
                generate_code_approx (n->creature->quest.code, &g->dic_places, r);
                n->creature->quest.open = 1;
                r = compute_scaled_rarity (n->creature->quest.code, &g->dic_places);
                n->creature->quest.bounty = 5.0 / (r * r);
                n->creature->quest.show_bounty = random_range (0, 1);
            }
        }
    }
}


/* input commands */
static void ic_where (struct game *g, struct player *p, int n_args, char **args)
{
    unsigned int i;
    char name[MAX_NAME_LENGTH] = {0};
    struct node *n = NULL;

    n = &g->w.nodes[p->node];
    get_name (name, n->code, &g->dic_places);
    fprintf (g->out, ">> %d: %s.\n", p->node, name);
    fflush (g->out);

    if (n->creature) {
        get_name (name, n->creature->code, &g->dic_creatures);
        fprintf (g->out, "## %s\n", name);
        fflush (g->out);
        if (n->creature->quest.open) {
            get_name (name, n->creature->quest.code, &g->dic_places);
            fprintf (g->out, "§§ %s\n", name);
            fflush (g->out);
            fprintf (g->out, "  Bounty: ");
            if (n->creature->quest.show_bounty)
                fprintf (g->out, "%d goldz.\n", n->creature->quest.bounty);
            else
                fprintf (g->out, "unknown.\n");
            fflush (g->out);
        } else if (n->creature->align == ENEMY) {
            fprintf (g->out, "  Foe.\n");
            fflush (g->out);
        }
    }
    for (i = 0; i < n->n_neighbors; i++) {
        unsigned int id = n->neighbors[i];
        get_name (name, g->w.nodes[id].code, &g->dic_places);
        fprintf (g->out, " > %d - %s.\n", id, name);
        fflush (g->out);
    }
}


static void ic_goto (struct game *g, struct player *p, int n_args, char **args)
{
    unsigned int i;
    struct node *n;
    long number;

    if (n_args != 1)
        return;

    number = strtol (args[0], NULL, 10);
    n = &g->w.nodes[p->node];
    for (i = 0; i < n->n_neighbors; i++) {
        if (number == n->neighbors[i]) {
            p->node = (unsigned int)number;
            number = -1;
            break;
        }
    }
    if (number == -1) {
        grow_world (g, p->node);
        ic_where (g, p, 0, NULL);
    } else {
        fprintf (g->out, "Dude pls.\n");
        fflush (g->out);
    }
}

#define NUM_INPUT_COMMANDS 2

struct input_command {
    char *name;
    void (*func)(struct game*, struct player*, int, char**);
};

static struct input_command input_commands[NUM_INPUT_COMMANDS];

static void init_input_commands (void)
{
    int i = 0;
#define SET_CMD(n, f) do { input_commands[i].name = n; input_commands[i].func = f; i++; } while (0)
    SET_CMD ("where", ic_where);
    SET_CMD (">", ic_goto);
}

static char* find_nickname (char *input)
{
    while (*input++ != '<');
    return input;
}
static void copy_nickname (char *input, char *output)
{
    input = find_nickname (input);
    while (*input != '>' && *input) {
        *output = *input;
        output++; input++;
    }
    *output = 0;
}
static char* find_message (char *input)
{
    while (*input && *input != '>') input++;
    if (*input && input[1])
        return &input[2];
    else
        return NULL;
}

static struct player* player_exists (struct game *g, const char *name)
{
    unsigned int i;
    for (i = 0; i < g->n_players; i++) {
        if (!strncmp (g->players[i].name, name, strlen (name)))
            return &g->players[i];
    }
    return NULL;
}

static void register_player (struct game *g, const char *name)
{
    struct player *p = NULL;

    if (g->n_players >= NUM_MAX_PLAYERS) {
        fprintf (g->out, "Maximum number of players reached, get lost %s.\n", name);
        fflush (g->out);
        return;
    }
    p = &g->players[g->n_players];
    strcpy (p->name, name);
    g->n_players++;
    fprintf (g->out, "Welcome to the adventure %s!\n", name);
    fflush (g->out);
}

static char* next_arg (char *s)
{
    while (!isspace (*s) && *s) s++;
    /* trickery. */
    if (*s) {
        *s = 0;
        s++;
        while (isspace (*s)) s++;
    }
    return s;
}

#define NUM_MAX_ARGS 6

static void parse_command (struct game *game, char *nickname, char *input)
{
    unsigned int i;
    char *args[NUM_MAX_ARGS];
    char *ptr = NULL;
    unsigned int n_args = 0;
    struct player *player = NULL;

    player = player_exists (game, nickname);
    if (strncmp ("lefuneste", input, strlen ("lefuneste"))) {
        if (!player)
            return;
    } else {
        if (!player)
            register_player (game, nickname);
        return;
    }

    for (i = 0; i < NUM_INPUT_COMMANDS; i++) {
        if (!strncmp (input, input_commands[i].name,
                      strlen (input_commands[i].name))) {
            /* read arguments */
            ptr = input;
            while (*ptr) {
                ptr = next_arg (ptr);
                if (*ptr != 0 && n_args < NUM_MAX_ARGS) {
                    args[n_args] = ptr;
                    n_args++;
                }
            }
            /* call command */
            input_commands[i].func (game, player, n_args, args);
            break;
        }
    }
}

static void sflush (char *s)
{
    while (*s != 0 && *s != '\n')
        *s++;
    *s = 0;
}


#define IRC_MODE

#define IRC_CHANNEL "#potager2"

int main (int argc, char **argv)
{
    struct game game;
    int playing = 1;

    init_game (&game);
    read_dictionary ("places.dic", &game.dic_places);
    read_dictionary ("creatures.dic", &game.dic_creatures);
    srand (time (NULL));

#define BASE_NODES 4
    game.w.allocated_nodes = BASE_NODES * sizeof (struct node);
    game.w.nodes = array_realloc (game.w.nodes, 0, game.w.allocated_nodes);

    /* generate the first node */
    game.p.node = 0;
    generate_node (&game.w, 0, &game.dic_places);

#ifdef IRC_MODE
    /* open streams */
    game.in = fopen ("irc.freenode.org/"IRC_CHANNEL"/out", "r");
    game.out = fopen ("irc.freenode.org/"IRC_CHANNEL"/in", "w");

    if (!game.in || !game.out) {
        perror ("fopen");
        return 1;
    }

    fprintf (game.out, "salut les cons\n");
    fflush (game.out);

    /* generate the first node */
    grow_world (&game, 0);

    init_input_commands ();

    while (playing) {
        char input[512], name[MAX_NAME_LENGTH];

        memset (input, 0, sizeof input);
        fgets (input, sizeof input - 1, game.in);
        sflush (input);

        if (*input) {
            char nickname[MAX_NAME_LENGTH];
            char *msg = NULL;
            msg = find_message (input);
            if (msg) {
                copy_nickname (input, nickname);
                if (*msg == '!') {
                    parse_command (&game, nickname, &msg[1]);
                    printf ("parsing command %s from %s\n", &msg[1], nickname);
                }
            }
        }
    }
#else
    /* welcome message */
    printf ("Welcome. You are %s, whether you like it or not.\n"
            "Type in /quit or /exit to quit the game.\n"
            "Enjoy!\n---------------------\n\n", game.p.name);


    while (playing) {
        unsigned int i;
        struct node *n = NULL;
        char input[MAX_INPUT_SIZE], name[MAX_NAME_LENGTH];
        int waiting_input = 1;

        /* generate the current node */
        grow_world (&game, game.p.node);
        n = &game.w.nodes[game.p.node];

        /* print message */
        get_name (name, n->code, &game.dic_places);
        printf ("---------\n");
        printf ("You have %d goldz.\n", game.p.goldz);
        printf ("You are in %d: %s.\n", game.p.node, name);
        if (n->creature) {
            get_name (name, n->creature->code, &game.dic_creatures);
            printf ("There is %s here!\n", name);
            if (n->creature->quest.open) {
                get_name (name, n->creature->quest.code, &game.dic_places);
                printf ("This adorable creature has a quest for you! "
                        "you must find: %s\n", name);
                printf ("Bounty: ");
                if (n->creature->quest.show_bounty)
                    printf ("%d goldz.\n", n->creature->quest.bounty);
                else
                    printf ("unknown.\n");
                printf ("Type in the number of such a place "
                        "preceded by a question mark '?' to fulfill the quest.\n");
            } else if (n->creature->align == ENEMY) {
                printf ("This creature doesnt look friendly.\n"
                        "Type in an exclamation mark '!' to engage in a fight with it.\n");
            }
        }
        printf ("You can go to:\n");
        for (i = 0; i < n->n_neighbors; i++) {
            unsigned int id = n->neighbors[i];
            get_name (name, game.w.nodes[id].code, &game.dic_places);
            printf (" %d - %s.\n", id, name);
        }
        printf ("Type in the number of the place you want to go to: ");
        fflush (stdout);

        /* wait input */
        while (waiting_input) {
            long number;
            memset (input, 0, MAX_INPUT_SIZE);
            fgets (input, MAX_INPUT_SIZE, stdin);
            switch (*input) {
            case '?':
                if (!n->creature || !n->creature->quest.open) {
                    printf ("There is no quest to do here!\n");
                    break;
                }
                number = strtol (&input[1], NULL, 10);
                if (number < game.w.n_nodes) {
                    if (is_code_included (game.w.nodes[number].code, n->creature->quest.code)) {
                        n->creature->quest.open = 0;
                        game.p.goldz += n->creature->quest.bounty;
                        printf ("Congratulations! You have successfully "
                                "completed the quest!\nYou have earned: %d goldz\n",
                                n->creature->quest.bounty);
                    } else {
                        printf ("Hmmm, this does look like the place the creature was looking for.\n");
                    }
                    waiting_input = 0;
                }
                break;
            case '/':
                if (!strncmp (&input[1], "quit", 4) || !strncmp (&input[1], "exit", 4)) {
                    playing = 0;
                    waiting_input = 0;
                }
                break;
            case '!':
                if (!n->creature || n->creature->align != ENEMY) {
                    printf ("There is no one to fight here!\n");
                    break;
                }
                printf ("Into the fight you go!\n");
                if (fight_loop (&game.p, &n->creature->fight)) {
                    /* player won, remove the creature */
                    clear_creature (n->creature);
                    free (n->creature);
                    n->creature = NULL;
                    /* restore hp of the player */
                    game.p.fight.health = game.p.fight.health_pool;
                    waiting_input = 0;
                } else {
                    waiting_input = 0;
                    playing = 0;
                    printf ("Game over.\n");
                }
                break;
            default:
                number = strtol (input, NULL, 10);
                for (i = 0; i < n->n_neighbors; i++) {
                    if (number == n->neighbors[i]) {
                        game.p.node = (unsigned int) number;
                        waiting_input = 0;
                        break;
                    }
                }
            }

            if (waiting_input) {
                printf ("Wrong input. Try again: ");
                fflush (stdout);
            }
        }
    }

    printf ("Your game has not been saved, hope you dont mind. Cya.\n");

    free (game.w.nodes);
#endif  /* IRC_MODE */

    return 0;
}

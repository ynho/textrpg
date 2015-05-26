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

struct creature {
    int code[2][MAX_WORDS];

    /* various retarded attributes */
    unsigned int health_pool;
    float health;               /* between 0 and 1 */
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
    struct creature creature;
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

static void init_creature (struct creature *c)
{
    unsigned int i;
    init_code (c->code);
    c->health_pool = 100;
    c->health = 1.0;
    c->align = NEUTRAL;
    init_quest (&c->quest);
}
static void clear_creature (struct creature *c)
{
    (void)c;
}

static void init_player (struct player *p)
{
    init_creature (&p->creature);
    memset (p->name, 0, MAX_NAME_LENGTH);
    strcpy (p->name, "Jean-Claude"); /* deal with it. */
    p->node = 0;
    p->goldz = 0;
}
static void clear_player (struct player *p)
{
    clear_creature (&p->creature);
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
    const unsigned int max_tries = 5000; /* TODO: we might wanna incrase that */

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

int main (int argc, char **argv)
{
    struct world w;
    struct player p;
    struct dictionary dic_places, dic_creatures;

    int playing = 1;

    init_world (&w);
    init_player (&p);
    init_dictionary (&dic_places);
    read_dictionary ("places.dic", &dic_places);
    init_dictionary (&dic_creatures);
    read_dictionary ("creatures.dic", &dic_creatures);
    srand (time (NULL));

#define BASE_NODES 4
    w.allocated_nodes = BASE_NODES * sizeof (struct node);
    w.nodes = array_realloc (w.nodes, 0, w.allocated_nodes);

    /* generate the first node */
    p.node = 0;
    generate_node (&w, 0, &dic_places);

    /* welcome message */
    printf ("Welcome. You are %s, whether you like it or not.\n"
            "Type in /quit or /exit to quit the game.\n"
            "Enjoy!\n---------------------\n\n", p.name);


#define CREATURE_CHANCE 30

    while (playing) {
        unsigned int i;
        struct node *n = NULL;
#define MAX_INPUT_SIZE 32
        char input[MAX_INPUT_SIZE], name[MAX_NAME_LENGTH];
        int waiting_input = 1;

        /* generate the current node */
        n = &w.nodes[p.node];
        if (!n->generated) {
            generate_neighbors (&w, p.node, &dic_places);
            n = &w.nodes[p.node]; /* generate_neighbors() might reallocate */
            if (random_range (1, 100) <= CREATURE_CHANCE) {
                generate_creature (&w, p.node, &dic_creatures);
                n->creature->align = random_range (0, ENEMY);

                if (/* some random && n->creature->align == FRIENDLY */1) {
                    /* pick a rarity number */
                    float r = compute_scaled_rarity (n->creature->code, &dic_creatures);
                    /* generate SOMETHING */
                    generate_code_approx (n->creature->quest.code, &dic_places, r);
                    n->creature->quest.open = 1;
                    r = compute_scaled_rarity (n->creature->quest.code, &dic_places);
                    n->creature->quest.bounty = 5.0 / (r * r);
                    n->creature->quest.show_bounty = random_range (0, 1);
                }
            }
        }

        /* print message */
        get_name (name, n->code, &dic_places);
        printf ("---------\n");
        printf ("You have %d goldz.\n", p.goldz);
        printf ("You are in %d: %s.\n", p.node, name);
        if (n->creature) {
            get_name (name, n->creature->code, &dic_creatures);
            printf ("There is %s here!\n", name);
            if (n->creature->quest.open) {
                get_name (name, n->creature->quest.code, &dic_places);
                printf ("This adorable creature has a quest for you! "
                        "you must find: %s\n", name);
                printf ("Bounty: ");
                if (n->creature->quest.show_bounty)
                    printf ("%d goldz.\n", n->creature->quest.bounty);
                else
                    printf ("unknown.\n");
                printf ("Type in the number of such a place "
                        "preceded by a question mark '?' to fulfill the quest.\n");
            }
        }
        printf ("You can go to:\n");
        for (i = 0; i < n->n_neighbors; i++) {
            unsigned int id = n->neighbors[i];
            get_name (name, w.nodes[id].code, &dic_places);
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
                if (number < w.n_nodes) {
                    if (is_code_included (w.nodes[number].code, n->creature->quest.code)) {
                        n->creature->quest.open = 0;
                        p.goldz += n->creature->quest.bounty;                        
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
            default:
                number = strtol (input, NULL, 10);
                for (i = 0; i < n->n_neighbors; i++) {
                    if (number == n->neighbors[i]) {
                        p.node = (unsigned int) number;
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

    free (w.nodes);

    return 0;
}

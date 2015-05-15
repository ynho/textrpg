/* text-based rpg */

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_WORD_LENGTH 40
/* the +20 is to hold "the", "of" and such */
#define MAX_NAME_LENGTH (5 * MAX_WORD_LENGTH + 20)
#define MAX_NEIGHBORS 4
#define MAX_WORDS 10

struct creature {
    int code[2][MAX_WORDS];

    /* various retarded attributes */
    unsigned int health_pool;
    float health;               /* between 0 and 1 */
    
};

struct node {
    int code[2][MAX_WORDS];
    int neighbors[MAX_NEIGHBORS];
    unsigned int n_neighbors;
    int generated;

    struct creature *creature;    
};

struct player {
    struct creature creature;
    char name[MAX_NAME_LENGTH];
    unsigned int node;

    /* TODO: inventory, equipped items, etc. */
};

struct world {
    /* nodes */
    unsigned int n_nodes, allocated_nodes;
    struct node *nodes;
    unsigned int n_not_generated;
};


static void init_creature (struct creature *c)
{
    unsigned int i;
    for (i = 0; i < MAX_WORDS; i++) {
        c->code[0][i] = 0;
        c->code[1][i] = 0;
    }
    c->health_pool = 100;
    c->health = 1.0;
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
}
static void clear_player (struct player *p)
{
    clear_creature (&p->creature);
}

static void init_node (struct node *n)
{
    unsigned int i;
    for (i = 0; i < MAX_WORDS; i++) {
        n->code[0][i] = 0;
        n->code[1][i] = 0;
    }
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
    r *= (b - a);
    r += a;
    return r + 0.5;
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


static void generate_code (int code[2][MAX_WORDS], struct dictionary *dic)
{
    int i;

    for (i = 0; i < MAX_WORDS; i++)
        code[0][i] = dic->probabilities[i];

    /* convert probabilities into booleans (very clean code there) */
    for (i = 0; i < dic->sentence_length; i++)
        code[0][i] = (random_range (1, 100) <= code[0][i]) ? 1 : 0;

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

    /* select the words */
    for (i = 0; i < dic->sentence_length; i++) {
        if (code[0][i])
            code[1][i] = random_range (0, dic->data[code[0][i] - 1][i].n_words - 1);
    }
}

static void get_name (char *name, int code[2][MAX_WORDS], struct dictionary *dic)
{
    int i;

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
    unsigned int i;
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
    char *msg = "no error";
    char buffer[1024] = {0};          /* TODO: let's hope this is big enough */
    char *end = NULL, *end2 = NULL;
    long a, b, position;
    int i, j, k;
    int counter[NUM_RARES][MAX_WORDS];

    /* sentence length */
    fgets (buffer, sizeof buffer, fp);
    a = strtol (buffer, &end, 10);
    if (buffer == end || a > MAX_WORDS) {
        msg = "bad sentence length";
        goto fail;
    }
    dic->sentence_length = a;

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
        dic->probabilities[i] = a;
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
        dic->rarity[i] = a;
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
    while (end2 = fgets (buffer, sizeof buffer, fp)) {
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
    while (end2 = fgets (buffer, sizeof buffer, fp)) {
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
    int i, j;

    fprintf (fp, "%d\n", dic->sentence_length);
    for (i = 0; i < dic->sentence_length; i++)
        fprintf (fp, "%s%d", (i == 0 ? "" : " "), dic->probabilities[i]);
    fprintf (fp, "\n");
    for (i = 0; i < dic->sentence_length; i++)
        fprintf (fp, "%s\n", dic->articles[i]);
    for (i = 0; i < NUM_RARES; i++) {
        for (j = 0; j < dic->sentence_length; j++) {
            int k;
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
    int i;

    for (i = 0; i < dic->sentence_length; i++) {
        if (!code[0][i])
            r *= 1.0 - dic->probabilities[i] / 100.0;
        else {
            r *= dic->probabilities[i] / 100.0;
            r *= dic->rarity[NUM_RARES - code[0][i]] / 100.0;
        }
    }

    return r;
}

static float compute_scaled_rarity (int code[2][MAX_WORDS], struct dictionary *dic)
{
    float r = 1.0;
    int most[2][MAX_WORDS];
    int i;

    for (i = 0; i < dic->sentence_length; i++)
        most[0][i] = most[1][i] = 0;

    /* get most probable code */
    for (i = 0; i < dic->sentence_length; i++) {
        if (dic->probabilities[i] > 50)
            most[0][i] = 1;
    }
    r = compute_rarity (most, dic);

    return compute_rarity (code, dic) / r;
}

/* is a included in b? */
static int is_code_included (int a[2][MAX_WORDS], int b[2][MAX_WORDS])
{
    int i;

    for (i = 0; i < MAX_WORDS; i++) {
        if (b[0][i]) {
            if (a[0][i] != b[0][i] || a[1][i] != b[1][i])
                return 0;
        }
    }

    return 1;
}

int main (int *argc, char **argv)
{
    struct world w;
    struct player p;
    struct dictionary dic_places, dic_creatures;

    const size_t node_size = sizeof (struct node);

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
            "Enjoy the game!\n---------------\n\n", p.name);



    {
        int a[2][MAX_WORDS], b[2][MAX_WORDS];
        int i;
        char namea[MAX_NAME_LENGTH] = {0};
        char nameb[MAX_NAME_LENGTH] = {0};
        for (i = 0; i < MAX_WORDS; i++)
            a[0][i] = a[1][i] = b[0][i] = b[1][i] = 0;
        a[0][2] = 1;
        a[1][2] = 3;

        b[0][0] = 2;
        b[0][2] = 1;
        b[1][2] = 3;

        get_name (namea, a, &dic_places);
        get_name (nameb, b, &dic_places);
        printf ("%s INCLUDED IN %s: %s\n", nameb, namea, is_code_included (b, a) ? "YES" : "NO");
    }

#define CREATURE_CHANCE 30

    while (playing) {
        unsigned int i;
        struct node *n = NULL;
#define MAX_INPUT_SIZE 32
        char input[MAX_INPUT_SIZE], name[MAX_NAME_LENGTH];
        int waiting_input = 1;

        /* generate the current node */
        generate_neighbors (&w, p.node, &dic_places);
        if (random_range (1, 100) <= CREATURE_CHANCE)
            generate_creature (&w, p.node, &dic_creatures);
        n = &w.nodes[p.node];

        /* print message */
        get_name (name, n->code, &dic_places);
        printf ("You are in %d: %s. %.4f\n", p.node, name, compute_scaled_rarity (n->code, &dic_places));
        if (n->creature) {
            get_name (name, n->creature->code, &dic_creatures);
            printf ("There is %s here! %.4f\n", name, compute_scaled_rarity (n->creature->code, &dic_creatures));
        }
        printf ("You can go to:\n");
        for (i = 0; i < n->n_neighbors; i++) {
            unsigned int id = n->neighbors[i];
            get_name (name, w.nodes[id].code, &dic_places);
            printf (" %d - %s.\n", id, name);
        }
        printf ("--------\nType in the number of the place you want to go to: ");
        fflush (stdout);

        /* wait input */
        while (waiting_input) {
            memset (input, 0, MAX_INPUT_SIZE);
            fgets (input, MAX_INPUT_SIZE, stdin);
            if (!strncmp (input, "quit", 4) || !strncmp (input, "exit", 4)) {
                playing = 0;
                waiting_input = 0;
            } else {
                long target = strtol (input, NULL, 10);
                for (i = 0; i < n->n_neighbors; i++) {
                    if (target == n->neighbors[i]) {
                        p.node = target;
                        waiting_input = 0;
                        break;
                    }
                }
                if (waiting_input) {
                    printf ("Wrong input. Try again: ");
                    fflush (stdout);
                }
            }
        }
    }

    printf ("Your game has not been saved, hope you dont mind. Cya.\n");

    free (w.nodes);

    return 0;
}

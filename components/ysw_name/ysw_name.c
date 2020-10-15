// Copyright 2020 Anthony F. Stuart - All rights reserved.
//
// This program and the accompanying materials are made available
// under the terms of the GNU General Public License. For other license
// options please contact the copyright owner.
//
// This program is made available on an "as is" basis, without
// warranties or conditions of any kind, either express or implied.

#include "ysw_name.h"

#include "esp_log.h"
#include "esp_system.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define TAG "YSW_NAME"

static const char *adjectives[] = {
    "Absorbing",
    "Accompanying",
    "Accomplished",
    "Acoustic",
    "Adorable",
    "Adventurous",
    "Affecting",
    "Aggressive",
    "Agreeable",
    "Airy",
    "Alive",
    "Alluring",
    "Alto",
    "Ambient",
    "Amplified",
    "Amused",
    "Amusing",
    "Angry",
    "Animating",
    "Annoying",
    "Anticipative",
    "Anxious",
    "Arranged",
    "Arrogant",
    "Artistic",
    "Ashamed",
    "Atmospheric",
    "Attractive",
    "Attuned",
    "Auditory",
    "Aural",
    "Average",
    "Awful",
    "Background",
    "Bad",
    "Band",
    "Baritone",
    "Baroque",
    "Bass",
    "Basso",
    "Beautiful",
    "Better",
    "Bewildered",
    "Blissful",
    "Blue",
    "Bluegrass",
    "Blues",
    "Bluesy",
    "Bombastic",
    "Boogie",
    "Booming",
    "Bopping",
    "Brainy",
    "Brassy",
    "Brava",
    "Brave",
    "Bravo",
    "Breakable",
    "Breezy",
    "Bright",
    "Brisk",
    "Buoyant",
    "Busy",
    "Calm",
    "Canonic",
    "Captivating",
    "Carefree",
    "Careful",
    "Catchy",
    "Cautious",
    "Chamber",
    "Chanting",
    "Charming",
    "Cheerful",
    "Chiming",
    "Choral",
    "Classic",
    "Classical",
    "Clean",
    "Clear",
    "Clever",
    "Cloudy",
    "Clumsy",
    "Colorful",
    "Combative",
    "Comfortable",
    "Comforting",
    "Compelling",
    "Complex",
    "Composed",
    "Concert",
    "Conducted",
    "Confused",
    "Contemporary",
    "Cool",
    "Cooperative",
    "Country",
    "Courageous",
    "Crazy",
    "Creepy",
    "Crowded",
    "Cruel",
    "Cute",
    "Dance",
    "Dangerous",
    "Dark",
    "Dead",
    "Deep",
    "Defiant",
    "Delightful",
    "Determined",
    "Different",
    "Difficult",
    "Distinct",
    "Disturbed",
    "Diverting",
    "Dizzy",
    "Double Time",
    "Drab",
    "Dramatic",
    "Dull",
    "Dynamic",
    "Eager",
    "Easy",
    "Easy Listening",
    "Elated",
    "Electronic",
    "Elegant",
    "Elemental",
    "Elevating",
    "Emotional",
    "Enchanting",
    "Encouraging",
    "Energetic",
    "Energizing",
    "Engaging",
    "Engrossing",
    "Enjoyable",
    "Entertaining",
    "Enthralling",
    "Enthusiastic",
    "Enticing",
    "Entrancing",
    "Envious",
    "Epic",
    "Essential",
    "Even",
    "Evil",
    "Evocative",
    "Excited",
    "Exciting",
    "Exhilarating",
    "Expensive",
    "Expressive",
    "Exuberant",
    "Fair",
    "Faithful",
    "Famous",
    "Fancy",
    "Fantastic",
    "Fast",
    "Feel Good",
    "Fierce",
    "Fine",
    "Flamboyant",
    "Flourishing",
    "Folk",
    "Folksy",
    "Foolish",
    "Foot Stomping",
    "Fragile",
    "Frail",
    "Frantic",
    "Fresh",
    "Friendly",
    "Full Toned",
    "Fun",
    "Funk",
    "Funky",
    "Funny",
    "Fusion",
    "Gentle",
    "Gifted",
    "Glamorous",
    "Gleaming",
    "Gleeful",
    "Glorious",
    "Golden",
    "Good",
    "Gorgeous",
    "Gospel",
    "Graceful",
    "Grandiose",
    "Grieving",
    "Grooving",
    "Groovy",
    "Grotesque",
    "Grumpy",
    "Handsome",
    "Happy",
    "Harmonic",
    "Harmonious",
    "Harmonized",
    "Healthy",
    "Heartfelt",
    "Heartsome",
    "Heavy Metal",
    "Heuristic",
    "Hi-Fi",
    "Hilarious",
    "Hip",
    "Hip-Hop",
    "Hit",
    "Homely",
    "Homey",
    "Hungry",
    "Hymnal",
    "Hypnotic",
    "Impassioned",
    "Important",
    "Impossible",
    "Impressive",
    "Improvisational",
    "Improvised",
    "Indie",
    "Innocent",
    "Inspiring",
    "Instrumental",
    "Intoned",
    "Intoxicating",
    "Intricate",
    "Intrinsic",
    "Invigorating",
    "Inviting",
    "Jammin",
    "Jaunty",
    "Jazzy",
    "Jittery",
    "Jive",
    "Jolly",
    "Joyful",
    "Joyous",
    "Jubilant",
    "Kind",
    "Lazy",
    "Light",
    "Lighthearted",
    "Lilting",
    "Live",
    "Lively",
    "Lonely",
    "Long",
    "Loud",
    "Lovely",
    "Lucky",
    "Lyric",
    "Lyrical",
    "Magnetic",
    "Magnificent",
    "Major",
    "Marching",
    "Masterly",
    "Measured",
    "Mellow",
    "Melodic",
    "Melodious",
    "Merry",
    "Mesmerizing",
    "Metal",
    "Meticulous",
    "Metrical",
    "Mirthful",
    "Misty",
    "Modern",
    "Monophonic",
    "Moody",
    "Motionless",
    "Motivating",
    "Motivational",
    "Moving",
    "Muddy",
    "Multifarious",
    "Mushy",
    "Musical",
    "Mysterious",
    "Nasty",
    "Naughty",
    "Nervous",
    "New Age",
    "Nice",
    "Nostalgic",
    "Notable",
    "Nutty",
    "Obedient",
    "Obnoxious",
    "Odd",
    "Old Fashioned",
    "Open",
    "Operatic",
    "Optimistic",
    "Orchestral",
    "Outrageous",
    "Outstanding",
    "Panicky",
    "Passionate",
    "Peaceful",
    "Percussive",
    "Perfect",
    "Piercing",
    "Placid",
    "Plain",
    "Playable",
    "Playful",
    "Pleasant",
    "Pleasing",
    "Pleasurable",
    "Poetic",
    "Poised",
    "Polyphonic",
    "Poor",
    "Pop",
    "Popular",
    "Powerful",
    "Practiced",
    "Precious",
    "Priceless",
    "Prickly",
    "Primal",
    "Proficient",
    "Progressive",
    "Proud",
    "Provocative",
    "Pulsating",
    "Pulsing",
    "Putrid",
    "Puzzled",
    "Quaint",
    "Quick",
    "Raucous",
    "Real",
    "Recorded",
    "Recreational",
    "Reggae",
    "Rejuvenating",
    "Relaxing",
    "Relieved",
    "Reminiscent",
    "Renaissance",
    "Repeating",
    "Resonant",
    "Resounding",
    "Restorative",
    "Reverberant",
    "Reverberating",
    "Rhapsodic",
    "Rhythmic",
    "Rhythmical",
    "Rich",
    "Ringing",
    "Rising",
    "Rock",
    "Rockin",
    "Rock-n-Rollin",
    "Rollicking",
    "Rollin",
    "Romantic",
    "Rousing",
    "Scary",
    "Sensational",
    "Sentimental",
    "Session",
    "Shiny",
    "Shy",
    "Silly",
    "Silvery",
    "Siren",
    "Slammin",
    "Sleepy",
    "Slow",
    "Smiling",
    "Smooth",
    "Soft",
    "Solo",
    "Song",
    "Sonic",
    "Soothing",
    "Sophisticated",
    "Soprano",
    "Sore",
    "Soul",
    "Soulful",
    "Sparkling",
    "Splendid",
    "Spotless",
    "Steady",
    "Stimulating",
    "Stirring",
    "Stormy",
    "Strange",
    "Strident",
    "Strong",
    "Stupid",
    "Sublime",
    "Successful",
    "Super",
    "Surging",
    "Suspenseful",
    "Sweet",
    "Swing",
    "Swinging",
    "Symphonic",
    "Symphonious",
    "Synced",
    "Synchronized",
    "Talented",
    "Tame",
    "Tasty",
    "Tender",
    "Tenor",
    "Tense",
    "Textured",
    "Thankful",
    "Thoughtful",
    "Throbbing",
    "Thunderous",
    "Tight",
    "Timbral",
    "Timeless",
    "Tired",
    "Toe Tapping",
    "Tonal",
    "Tonic",
    "Touching",
    "Tough",
    "Transcendent",
    "Transporting",
    "Treble",
    "Trilling",
    "Troubled",
    "Tuneful",
    "Tympanic",
    "Unaccompanied",
    "Unifying",
    "Uninhibited",
    "Uniting",
    "Unplugged",
    "Unrestrained",
    "Unusual",
    "Upbeat",
    "Upbuilding",
    "Uplifting",
    "Upset",
    "Up Tempo",
    "Uptight",
    "Upwelling",
    "Vast",
    "Vibrant",
    "Victorious",
    "Virtuoso",
    "Vivacious",
    "Vocal",
    "Wandering",
    "Warbling",
    "Warm",
    "Wavy",
    "Weary",
    "Wicked",
    "Wide Eyed",
    "Wild",
    "Witty",
    "World",
    "Worried",
    "Zany",
    "Zealous",
};

#define ADJECTIVES_SZ (sizeof(adjectives) / sizeof(const char *))

const char *nouns[] = {
    "Aardvark",
    "Albatross",
    "Alligator",
    "Anchovy",
    "Ant",
    "Anteater",
    "Antelope",
    "Armadillo",
    "Baboon",
    "Badger",
    "Barnacle",
    "Barracuda",
    "Bass",
    "Bat",
    "Bear",
    "Beaver",
    "Bee",
    "Bird",
    "Bison",
    "Boa",
    "Boar",
    "Bobcat",
    "Bumblebee",
    "Bunny",
    "Butterfly",
    "Buzzard",
    "Camel",
    "Canary",
    "Caribou",
    "Cat",
    "Catfish",
    "Centipede",
    "Chameleon",
    "Cheetah",
    "Chicken",
    "Chimpanzee",
    "Chinchilla",
    "Chipmunk",
    "Cicada",
    "Clam",
    "Cobra",
    "Conch",
    "Condor",
    "Coral",
    "Cow",
    "Coyote",
    "Crab",
    "Crane",
    "Crayfish",
    "Cricket",
    "Crocodile",
    "Crow",
    "Deer",
    "Dog",
    "Dolphin",
    "Donkey",
    "Dove",
    "Dragonfly",
    "Duck",
    "Eagle",
    "Earthworm",
    "Eel",
    "Egret",
    "Elephant",
    "Emu",
    "Falcon",
    "Finch",
    "Firefly",
    "Fish",
    "Flamingo",
    "Flounder",
    "Fly",
    "Fox",
    "Frog",
    "Gazelle",
    "Gerbil",
    "Giraffe",
    "Goat",
    "Goldfish",
    "Goose",
    "Gorilla",
    "Grasshopper",
    "Gull",
    "Guppy",
    "Haddock",
    "Hamster",
    "Hare",
    "Hawk",
    "Hedgehog",
    "Heron",
    "Hippo",
    "Hornet",
    "Horse",
    "Hummingbird",
    "Hyena",
    "Iguana",
    "Jackal",
    "Jaguar",
    "Jay",
    "Jellyfish",
    "Kangaroo",
    "Kitten",
    "Koala",
    "Ladybug",
    "Lamb",
    "Lamprey",
    "Lark",
    "Lemur",
    "Leopard",
    "Lion",
    "Lizard",
    "Llama",
    "Lobster",
    "Locust",
    "Lynx",
    "Macaw",
    "Mackerel",
    "Magpie",
    "Meerkat",
    "Millipede",
    "Mink",
    "Minnow",
    "Mockingbird",
    "Mole",
    "Mongoose",
    "Monkey",
    "Moose",
    "Moth",
    "Mouse",
    "Muskrat",
    "Mussel",
    "Narwhal",
    "Nautilus",
    "Needlefish",
    "Newt",
    "Nightingale",
    "Octopus",
    "Opossum",
    "Orangutan",
    "Osprey",
    "Ostrich",
    "Otter",
    "Owl",
    "Ox",
    "Oyster",
    "Panda",
    "Parakeet",
    "Parrot",
    "Partridge",
    "Peacock",
    "Pelican",
    "Penguin",
    "Perch",
    "Pheasant",
    "Pig",
    "Pigeon",
    "Piranha",
    "Platypus",
    "Polecat",
    "Porcupine",
    "Porpoise",
    "Prawn",
    "Pronghorn",
    "Puppy",
    "Python",
    "Quail",
    "Rabbit",
    "Raccoon",
    "Ram",
    "Rat",
    "Rattlesnake",
    "Raven",
    "Ray",
    "Reindeer",
    "Rhino",
    "Roach",
    "Sailfish",
    "Salamander",
    "Salmon",
    "Scallop",
    "Scorpion",
    "Seahorse",
    "Seal",
    "Shark",
    "Sheep",
    "Shrew",
    "Shrimp",
    "Skunk",
    "Sloth",
    "Snail",
    "Snake",
    "Snapper",
    "Sparrow",
    "Spider",
    "Sponge",
    "Squid",
    "Squirrel",
    "Starfish",
    "Stingray",
    "Stork",
    "Swallow",
    "Swan",
    "Tarantula",
    "Tiger",
    "Toad",
    "Toucan",
    "Trout",
    "Tuna",
    "Turkey",
    "Turtle",
    "Vole",
    "Vulture",
    "Walleye",
    "Walrus",
    "Warthog",
    "Wasp",
    "Weasel",
    "Whale",
    "Whelk",
    "Wildebeest",
    "Wolf",
    "Wolverine",
    "Wombat",
    "Woodpecker",
    "Worm",
    "Wren",
    "Yak",
    "Zebra",
};

#define NOUNS_SZ (sizeof(nouns) / sizeof(const char *))

int ysw_name_create(char *name, uint32_t size)
{
    uint32_t adjective_index = esp_random() % ADJECTIVES_SZ;
    uint32_t noun_index = esp_random() % NOUNS_SZ;
    ESP_LOGD(TAG, "adjective_index=%d, noun_index=%d", adjective_index, noun_index);
    int rc = snprintf(name, size, "%s %s", adjectives[adjective_index], nouns[noun_index]);
    return rc;
}

int ysw_name_create_new_version(const char *old_name, char *new_name, uint32_t size)
{
    int version_point = ysw_name_find_version_point(old_name);
    int version = atoi(old_name + version_point) + 1; // works for any version_point
    return ysw_name_format_version(new_name, size, version_point, old_name, version);
}

int ysw_name_find_version_point(const char *name)
{
    int index = strlen(name) - 1;
    while (index > 0) {
        if (!('0' <= name[index] && name[index] <= '9')) {
            if (name[index] == '+' && index > 0 && name[index - 1] == ' ') {
                return index - 1;
            }
            return index + 1;
        }
        index--;
    }
    return 0;
}

int ysw_name_format_version(char *new_name, uint32_t size, uint32_t version_point, const char *old_name, uint32_t version)
{
    int rc = snprintf(new_name, size, "%.*s +%d", version_point, old_name, version);
    return rc;
}


// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The emoji sequences UTS #51 recommends for general interchange (RGI),
// from emoji-sequences.txt and emoji-zwj-sequences.txt, with the emoji
// properties of emoji-data.txt.
//
// Most of the set follows from the properties, and the generator checks
// that it does rather than storing it: the basic emoji are the code
// points with Emoji_Presentation but the regional indicators, and the
// other emoji with U+FE0F after them but the keycap bases; the keycaps
// are "#", "*" and the digits with U+FE0F and U+20E3; the modifier
// sequences are each Emoji_Modifier_Base with each Emoji_Modifier, but
// for the bases listed as taking none. What is stored: the flags as a
// bitmap over pairs of regional indicators, the tag sequences as their
// tag letters, and the ZWJ sequences as skeletons. A ZWJ sequence is
// elements joined by U+200D, each a base with U+FE0F when it is not
// emoji by default and takes no modifier, or a base and a modifier. A
// skeleton is the bases; it lists which elements take modifiers, all of
// them together and each any of the five, whether the sequence comes
// without them too, and whether the modifiers must all differ (a couple
// whose two tones are the same is a single code point with a modifier
// instead). Skeletons are grouped by their first base. If the data stops
// following these rules, the generator fails, so a new Unicode version
// cannot change what the library recognizes unseen.

#include "munigen.h"

#define MAX_SEQUENCE   16
#define MAX_BASES      256
#define MAX_SKELETONS  512
#define MAX_ELEMENTS   8
#define MAX_TAGS       8
#define MAX_TAG_LENGTH 8
#define MAX_VECTORS    32
#define MODIFIERS      5
#define FLAG_BYTES     ((26 * 26 + 7) / 8)
#define ZWJ            0x200Du
#define VS16           0xFE0Fu
#define KEYCAP         0x20E3u
#define BLACK_FLAG     0x1F3F4u
#define CANCEL_TAG     0xE007Fu
#define FIRST_MODIFIER 0x1F3FBu

typedef struct Skeleton
{
    uint8_t count;
    uint8_t bases[MAX_ELEMENTS];                // indexes into the base list
    uint8_t vectors[MAX_VECTORS][MAX_ELEMENTS]; // per element, 0 or the modifier's number, 1 to 5
    int vectorCount;
} Skeleton;

typedef struct Emoji
{
    uint8_t* presentation;
    uint8_t* emoji;
    uint8_t* modifierBase;
    uint8_t* modifier;
    uint8_t* basic;     // 1: alone, 2: with U+FE0F, 4: as a keycap base
    uint8_t* modifiers; // the modifiers a base takes in modifier sequences, as bits
    uint8_t flags[FLAG_BYTES];
    char tags[MAX_TAGS][MAX_TAG_LENGTH + 1];
    int tagCount;
    uint32_t bases[MAX_BASES]; // the bases of ZWJ elements, sorted
    int baseCount;
    Skeleton* skeletons;
    int skeletonCount;
} Emoji;

static bool IsRegionalIndicator(uint32_t c)
{
    return c >= 0x1F1E6 && c <= 0x1F1FF;
}

static bool IsKeycapBase(uint32_t c)
{
    return c == '#' || c == '*' || (c >= '0' && c <= '9');
}

// Parses the code points of a sequence field into out and returns how
// many there are.
static int ParseSequence(const char* text, uint32_t* out, const char* fileName)
{
    char copy[MAX_LINE];
    snprintf(copy, sizeof(copy), "%s", text);
    int count = 0;
    for (char* item = strtok(copy, " "); item != nullptr; item = strtok(nullptr, " "))
    {
        if (count == MAX_SEQUENCE)
        {
            Fail("a sequence longer than %d in %s", MAX_SEQUENCE, fileName);
        }
        out[count++] = ParseCodePoint(item, fileName);
    }
    return count;
}

static void AddModifierSequence(Emoji* data, const uint32_t* s, int n)
{
    if (n != 2 || !data->modifierBase[s[0]] || !data->modifier[s[1]] ||
        s[1] - FIRST_MODIFIER >= MODIFIERS)
    {
        Fail("U+%04X: a modifier sequence that is not a base and a modifier", s[0]);
    }
    data->modifiers[s[0]] |= (uint8_t)(1u << (s[1] - FIRST_MODIFIER));
}

static void AddFlag(Emoji* data, const uint32_t* s, int n)
{
    if (n != 2 || !IsRegionalIndicator(s[0]) || !IsRegionalIndicator(s[1]))
    {
        Fail("U+%04X: a flag that is not two regional indicators", s[0]);
    }
    unsigned bit = (s[0] - 0x1F1E6) * 26 + (s[1] - 0x1F1E6);
    data->flags[bit / 8] |= (uint8_t)(1u << (bit % 8));
}

static void AddTag(Emoji* data, const uint32_t* s, int n)
{
    if (n < 3 || n - 2 > MAX_TAG_LENGTH || s[0] != BLACK_FLAG || s[n - 1] != CANCEL_TAG ||
        data->tagCount == MAX_TAGS)
    {
        Fail("U+%04X: a tag sequence the library cannot store", s[0]);
    }
    char* tag = data->tags[data->tagCount++];
    for (int i = 1; i < n - 1; i++)
    {
        if (s[i] < 0xE0020 || s[i] > 0xE007E)
        {
            Fail("U+%04X in a tag sequence is no tag character", s[i]);
        }
        tag[i - 1] = (char)(s[i] - 0xE0000);
    }
    tag[n - 2] = '\0';
}

static void AddBasic(Emoji* data, const uint32_t* s, int n)
{
    if (n == 1)
    {
        data->basic[s[0]] |= 1;
    }
    else if (n == 2 && s[1] == VS16)
    {
        data->basic[s[0]] |= 2;
    }
    else
    {
        Fail("U+%04X: a basic emoji of more than a code point and U+FE0F", s[0]);
    }
}

static void LoadSequences(Emoji* data)
{
    FILE* file = OpenUcd("emoji-sequences.txt");
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[3];
        if (SplitFields(line, fields, 3) < 2 || fields[0][0] == '\0')
        {
            continue;
        }
        uint32_t s[MAX_SEQUENCE] = {0};
        if (strstr(fields[0], "..") != nullptr)
        {
            uint32_t first;
            uint32_t last;
            ParseRange(fields[0], &first, &last, "emoji-sequences.txt");
            if (strcmp(fields[1], "Basic_Emoji") != 0)
            {
                Fail("a range of %s in emoji-sequences.txt", fields[1]);
            }
            for (uint32_t c = first; c <= last; c++)
            {
                data->basic[c] |= 1;
            }
            continue;
        }
        int n = ParseSequence(fields[0], s, "emoji-sequences.txt");
        if (strcmp(fields[1], "Basic_Emoji") == 0)
        {
            AddBasic(data, s, n);
        }
        else if (strcmp(fields[1], "Emoji_Keycap_Sequence") == 0)
        {
            if (n != 3 || !IsKeycapBase(s[0]) || s[1] != VS16 || s[2] != KEYCAP ||
                (data->basic[s[0]] & 4) != 0)
            {
                Fail("U+%04X: a keycap sequence of another form, or listed twice", s[0]);
            }
            data->basic[s[0]] |= 4;
        }
        else if (strcmp(fields[1], "RGI_Emoji_Flag_Sequence") == 0)
        {
            AddFlag(data, s, n);
        }
        else if (strcmp(fields[1], "RGI_Emoji_Modifier_Sequence") == 0)
        {
            AddModifierSequence(data, s, n);
        }
        else if (strcmp(fields[1], "RGI_Emoji_Tag_Sequence") == 0)
        {
            AddTag(data, s, n);
        }
        else
        {
            Fail("unknown sequence type '%s' in emoji-sequences.txt", fields[1]);
        }
    }
    fclose(file);
}

// The rules the data must follow for what is not stored.
static void CheckRules(const Emoji* data)
{
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        bool alone = data->presentation[c] && !IsRegionalIndicator(c);
        bool selected = data->emoji[c] && !data->presentation[c] && !IsKeycapBase(c);
        if (((data->basic[c] & 1) != 0) != alone || ((data->basic[c] & 2) != 0) != selected)
        {
            Fail("U+%04X: a basic emoji the rules do not give, or the other way round", c);
        }
        if (((data->basic[c] & 4) != 0) != IsKeycapBase(c))
        {
            Fail("U+%04X: a keycap the rules do not give, or the other way round", c);
        }
        uint8_t taken = data->modifiers[c];
        if (taken != 0 && taken != (1u << MODIFIERS) - 1)
        {
            Fail("U+%04X takes some modifiers but not all", c);
        }
    }
}

static uint8_t BaseIndex(Emoji* data, uint32_t base)
{
    for (int i = 0; i < data->baseCount; i++)
    {
        if (data->bases[i] == base)
        {
            return (uint8_t)i;
        }
    }
    if (data->baseCount == MAX_BASES)
    {
        Fail("more than %d bases in ZWJ sequences", MAX_BASES);
    }
    data->bases[data->baseCount] = base;
    return (uint8_t)data->baseCount++;
}

// Splits a ZWJ sequence into its bases and modifiers, checking the form
// of each element.
static int SplitElements(Emoji* data, const uint32_t* s, int n, uint32_t* bases, uint8_t* modifiers)
{
    int count = 0;
    int i = 0;
    while (i < n)
    {
        if (count == MAX_ELEMENTS)
        {
            Fail("U+%04X: a ZWJ sequence of more than %d elements", s[0], MAX_ELEMENTS);
        }
        uint32_t base = s[i++];
        uint8_t modifier = 0;
        bool selector = false;
        while (i < n && s[i] != ZWJ)
        {
            if (s[i] == VS16 && !selector && modifier == 0)
            {
                selector = true;
            }
            else if (data->modifier[s[i]] && modifier == 0 && !selector &&
                     s[i] - FIRST_MODIFIER < MODIFIERS)
            {
                modifier = (uint8_t)(s[i] - FIRST_MODIFIER + 1);
            }
            else
            {
                Fail("U+%04X: an element of a ZWJ sequence of another form", base);
            }
            i++;
        }
        if (selector != (!data->presentation[base] && modifier == 0))
        {
            Fail("U+%04X: U+FE0F where the rule does not put it in a ZWJ sequence", base);
        }
        bases[count] = base;
        modifiers[count++] = modifier;
        i += i < n ? 1 : 0;
        if (i == n && s[n - 1] == ZWJ)
        {
            Fail("U+%04X: a ZWJ sequence that ends with U+200D", s[0]);
        }
    }
    return count;
}

static void LoadZwjSequences(Emoji* data)
{
    FILE* file = OpenUcd("emoji-zwj-sequences.txt");
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* fields[3];
        if (SplitFields(line, fields, 3) < 2 || fields[0][0] == '\0')
        {
            continue;
        }
        if (strcmp(fields[1], "RGI_Emoji_ZWJ_Sequence") != 0)
        {
            Fail("unknown sequence type '%s' in emoji-zwj-sequences.txt", fields[1]);
        }
        uint32_t s[MAX_SEQUENCE] = {0};
        int n = ParseSequence(fields[0], s, "emoji-zwj-sequences.txt");
        uint32_t bases[MAX_ELEMENTS];
        uint8_t modifiers[MAX_ELEMENTS];
        int count = SplitElements(data, s, n, bases, modifiers);
        uint8_t indexes[MAX_ELEMENTS];
        for (int k = 0; k < count; k++)
        {
            indexes[k] = BaseIndex(data, bases[k]);
        }
        Skeleton* skeleton = nullptr;
        for (int k = 0; k < data->skeletonCount; k++)
        {
            Skeleton* other = &data->skeletons[k];
            if (other->count == count && memcmp(other->bases, indexes, (size_t)count) == 0)
            {
                skeleton = other;
            }
        }
        if (skeleton == nullptr)
        {
            if (data->skeletonCount == MAX_SKELETONS)
            {
                Fail("more than %d ZWJ skeletons", MAX_SKELETONS);
            }
            skeleton = &data->skeletons[data->skeletonCount++];
            skeleton->count = (uint8_t)count;
            memcpy(skeleton->bases, indexes, (size_t)count);
        }
        if (skeleton->vectorCount == MAX_VECTORS)
        {
            Fail("U+%04X: more than %d ZWJ sequences of one skeleton", s[0], MAX_VECTORS);
        }
        memcpy(skeleton->vectors[skeleton->vectorCount++], modifiers, (size_t)count);
    }
    fclose(file);
}

// Whether a skeleton has the sequence of modifiers vector.
static bool HasVector(const Skeleton* skeleton, const uint8_t* vector)
{
    for (int v = 0; v < skeleton->vectorCount; v++)
    {
        if (memcmp(skeleton->vectors[v], vector, skeleton->count) == 0)
        {
            return true;
        }
    }
    return false;
}

// The header of a skeleton: its element count in the low four bits, 16
// when it comes without modifiers, 32 when its modifiers must differ;
// and the mask of elements that take modifiers. Fails unless its
// sequences are exactly what these give.
static void Describe(const Skeleton* skeleton, uint8_t* headerOut, uint8_t* maskOut)
{
    uint8_t mask = 0;
    uint8_t plain[MAX_ELEMENTS] = {0};
    bool hasPlain = HasVector(skeleton, plain);
    for (int v = 0; v < skeleton->vectorCount; v++)
    {
        for (int k = 0; k < skeleton->count; k++)
        {
            mask |= (uint8_t)(skeleton->vectors[v][k] != 0 ? 1u << k : 0);
        }
    }
    int taking = 0;
    for (int k = 0; k < skeleton->count; k++)
    {
        taking += (mask >> k) & 1;
    }
    // Every assignment of modifiers to the masked elements, and whether
    // the skeleton has it: all of them, or those whose modifiers differ.
    int total = 1;
    for (int k = 0; k < taking; k++)
    {
        total *= MODIFIERS;
    }
    int all = 0;
    int distinct = 0;
    int distinctHad = 0;
    for (int n = 0; n < (mask != 0 ? total : 0); n++)
    {
        uint8_t vector[MAX_ELEMENTS] = {0};
        int rest = n;
        uint8_t seen = 0;
        bool differ = true;
        for (int k = 0; k < skeleton->count; k++)
        {
            if ((mask >> k) & 1)
            {
                vector[k] = (uint8_t)(rest % MODIFIERS + 1);
                rest /= MODIFIERS;
                differ = differ && (seen & (1u << vector[k])) == 0;
                seen |= (uint8_t)(1u << vector[k]);
            }
        }
        bool had = HasVector(skeleton, vector);
        all += had;
        distinct += differ;
        distinctHad += differ && had;
    }
    int listed = all + (hasPlain ? 1 : 0);
    bool everything = all == (mask != 0 ? total : 0);
    bool differing = mask != 0 && all == distinct && distinctHad == distinct;
    if (listed != skeleton->vectorCount || (!everything && !differing))
    {
        Fail("U+%04X: ZWJ sequences whose modifiers follow no rule", skeleton->bases[0]);
    }
    *headerOut = (uint8_t)(skeleton->count | (hasPlain ? 16 : 0) | (everything ? 0 : 32));
    *maskOut = mask;
}

static void WriteData(Emoji* data)
{
    FILE* file = CreateOutput("emoji_sequences.c");
    fprintf(file, "// SPDX-License-Identifier: MIT\n// Copyright (c) 2026 Sirac Ozmen\n//\n");
    WriteComment(file, "RGI emoji sequences: see src/tables.h. Generated by tools/munigen from "
                       "emoji-sequences.txt, emoji-zwj-sequences.txt and emoji-data.txt in "
                       "tools/ucd/ (Unicode 18.0.0); do not edit.");
    fprintf(file, "// clang-format off\n\n#include \"../tables.h\"\n\n");
    fprintf(file, "const uint8_t muniEmojiFlags[%d] = {", FLAG_BYTES);
    for (int i = 0; i < FLAG_BYTES; i++)
    {
        fprintf(file, "%s0x%02X,", i % 12 == 0 ? "\n    " : " ", data->flags[i]);
    }
    int unmodifiable = 0;
    uint32_t exceptions[16];
    for (uint32_t c = 0; c < CODE_POINTS; c++)
    {
        if (data->modifierBase[c] && data->modifiers[c] == 0)
        {
            if (unmodifiable == COUNT_OF(exceptions))
            {
                Fail("more than %d modifier bases take no modifier", COUNT_OF(exceptions));
            }
            exceptions[unmodifiable++] = c;
        }
        else if (!data->modifierBase[c] && data->modifiers[c] != 0)
        {
            Fail("U+%04X takes modifiers without Emoji_Modifier_Base", c);
        }
    }
    fprintf(file, "\n};\n\nconst uint32_t muniEmojiUnmodifiable[%d] = {",
            unmodifiable > 0 ? unmodifiable : 1);
    for (int i = 0; i < (unmodifiable > 0 ? unmodifiable : 1); i++)
    {
        fprintf(file, " 0x%05X,", i < unmodifiable ? exceptions[i] : 0);
    }
    fprintf(file, " };\nconst uint8_t muniEmojiUnmodifiableCount = %d;\n\n", unmodifiable);
    fprintf(file, "const char muniEmojiTags[%d][%d] = {", MAX_TAGS, MAX_TAG_LENGTH + 1);
    for (int i = 0; i < data->tagCount; i++)
    {
        fprintf(file, " \"%s\",", data->tags[i]);
    }
    fprintf(file, " };\nconst uint8_t muniEmojiTagCount = %d;\n\n", data->tagCount);

    // The bases, sorted, and the skeletons grouped by first base.
    uint32_t sorted[MAX_BASES];
    memcpy(sorted, data->bases, sizeof(uint32_t) * (size_t)data->baseCount);
    for (int i = 1; i < data->baseCount; i++)
    {
        for (int j = i; j > 0 && sorted[j - 1] > sorted[j]; j--)
        {
            uint32_t swap = sorted[j];
            sorted[j] = sorted[j - 1];
            sorted[j - 1] = swap;
        }
    }
    uint8_t rank[MAX_BASES];
    for (int i = 0; i < data->baseCount; i++)
    {
        for (int j = 0; j < data->baseCount; j++)
        {
            if (sorted[j] == data->bases[i])
            {
                rank[i] = (uint8_t)j;
            }
        }
    }
    fprintf(file, "const uint32_t muniEmojiZwjBases[%d] = {", data->baseCount);
    for (int i = 0; i < data->baseCount; i++)
    {
        fprintf(file, "%s0x%05X,", i % 8 == 0 ? "\n    " : " ", sorted[i]);
    }
    fprintf(file, "\n};\nconst uint16_t muniEmojiZwjBaseCount = %d;\n\n", data->baseCount);
    static uint8_t bytes[MAX_SKELETONS * (2 + MAX_ELEMENTS)];
    uint16_t starts[MAX_BASES + 1];
    int size = 0;
    for (int b = 0; b < data->baseCount; b++)
    {
        starts[b] = (uint16_t)size;
        for (int k = 0; k < data->skeletonCount; k++)
        {
            const Skeleton* skeleton = &data->skeletons[k];
            if (rank[skeleton->bases[0]] != b)
            {
                continue;
            }
            Describe(skeleton, &bytes[size], &bytes[size + 1]);
            size += 2;
            for (int e = 0; e < skeleton->count; e++)
            {
                bytes[size++] = rank[skeleton->bases[e]];
            }
        }
    }
    starts[data->baseCount] = (uint16_t)size;
    fprintf(file, "const uint16_t muniEmojiZwjStarts[%d] = {", data->baseCount + 1);
    for (int i = 0; i <= data->baseCount; i++)
    {
        fprintf(file, "%s%d,", i % 12 == 0 ? "\n    " : " ", starts[i]);
    }
    fprintf(file, "\n};\n\nconst uint8_t muniEmojiZwjSkeletons[%d] = {", size);
    for (int i = 0; i < size; i++)
    {
        fprintf(file, "%s0x%02X,", i % 12 == 0 ? "\n    " : " ", bytes[i]);
    }
    fprintf(file, "\n};\n");
    fclose(file);
    int total = FLAG_BYTES + 4 * (unmodifiable > 0 ? unmodifiable : 1) + 1 +
                MAX_TAGS * (MAX_TAG_LENGTH + 1) + 1 + 4 * data->baseCount + 2 +
                2 * (data->baseCount + 1) + size;
    printf("%-24s %7d bytes  %d flags bits, %d tags, %d bases, %d skeletons\n", "EmojiSequences",
           total, 26 * 26, data->tagCount, data->baseCount, data->skeletonCount);
}

void WriteEmojiSequences(void)
{
    static Emoji data;
    data.presentation = Allocate(CODE_POINTS);
    data.emoji = Allocate(CODE_POINTS);
    data.modifierBase = Allocate(CODE_POINTS);
    data.modifier = Allocate(CODE_POINTS);
    data.basic = Allocate(CODE_POINTS);
    data.modifiers = Allocate(CODE_POINTS);
    data.skeletons = Allocate(sizeof(Skeleton) * MAX_SKELETONS);
    LoadBinary("emoji-data.txt", "Emoji_Presentation", data.presentation);
    LoadBinary("emoji-data.txt", "Emoji", data.emoji);
    LoadBinary("emoji-data.txt", "Emoji_Modifier_Base", data.modifierBase);
    LoadBinary("emoji-data.txt", "Emoji_Modifier", data.modifier);
    LoadSequences(&data);
    CheckRules(&data);
    LoadZwjSequences(&data);
    WriteData(&data);
    free(data.presentation);
    free(data.emoji);
    free(data.modifierBase);
    free(data.modifier);
    free(data.basic);
    free(data.modifiers);
    free(data.skeletons);
}

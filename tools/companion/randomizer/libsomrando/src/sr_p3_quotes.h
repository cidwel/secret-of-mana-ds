/* libsomrando: the dialogue quote pools of Ancient Cave (AncientCaveEventMaker "acDialogue").  LGPL-2.1 or later (this header).
 *
 * The TEXT is not covered by the LGPL: MitchHedbergQuotes.cs and DemetriMartinQuotes.cs of the upstream project hold third-party quotes.  The text
 * lives only in data/sr_quotes_mitch.c and data/sr_quotes_demetri.c (see data/README.quotes.md); a build without them (SOMRANDO_QUOTES=OFF)
 * links data/sr_quotes_placeholder.c instead, with pools of the same sizes, and the generated ROMs then differ from the C# generator in the
 * dialogue bytes (and in the addresses that follow them).
 */
#ifndef SR_P3_QUOTES_H
#define SR_P3_QUOTES_H
extern const char *const sr_quotes_mitch[];
extern const int sr_quotes_mitch_count;
extern const char *const sr_quotes_demetri[];
extern const int sr_quotes_demetri_count;
extern const int sr_quotes_are_upstream;   /* 1 when the upstream text is linked (byte-identical output), 0 for the placeholders */
#endif

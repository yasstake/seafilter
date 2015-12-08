/* Copyright 2011 Bernhard R. Fischer, 2048R/5C5FFD47 <bf@abenteuerland.at>
 *
 * This file is part of smfilter.
 *
 * Smfilter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * Smfilter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with smfilter. If not, see <http://www.gnu.org/licenses/>.
 */

/*! This program reads an OSM/XML file and parses, filters, and modifies it.
 *  Filter and modification rules are hardcoded.
 *
 *  @author Bernhard R. Fischer
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include "osm_inplace.h"
#include "bstring.h"
#include "libhpxml.h"
#include "seamark.h"
#include "smlog.h"


int oline_ = 0;
int parse_rhint_ = 0;
int untagged_circle_ = 0;
int gen_lc_ = 0;
int gen_sec_ = 1;
double dir_arc_ = 2.0;


int match_node(const hpx_tree_t *t, bstring_t *b)
{
   int i, j;

   for (i = 0; i < t->nsub; i++)
   {
      for (j = 0; j < t->subtag[i]->tag->nattr; j++)
      {
         if (!bs_cmp(t->subtag[i]->tag->attr[j].name, "k"))
         {
            if (!bs_cmp(t->subtag[i]->tag->attr[j].value, "seamark:type"))
            {
               get_v(t->subtag[i]->tag, b);
               return 1;
            }
         }
      }
   }
   return 0;
}


void usage(const char *s)
{
   printf("Seamark filter V1.1, (c) 2011, Bernhard R. Fischer, <bf@abenteuerland.at>.\n\n"
          "This program reads an OSM file on stdin and adds sectors and arcs to\n"
          "seamarks. The result together with the input is output on stdout. If\n"
          "sectors are found without having start and/or end bearing defined an\n"
          "error message is included in the output within XML comment tags\n"
          "<!-- ERROR: ... -->.\n\n"
          "usage: %s [OPTIONS] [< inputfile] [> outputfile]\n"
          "   -a <dist> ...... Set maximum arc segment distance (default = %.2f nm).\n"
          "                    If this is set to 0, it is ignored.\n"
          "   -b <degrees> ... Set degrees (+/-) of arc for directional lights (default = %.1f deg).\n"
          "   -c ............. Generate nodes with 'seamark:light_character' tag.\n"
          "   -d <div> ....... Arc divisor (default = %.2f).\n"
          "   -h ............. This help.\n"
          "   -H ............. Parse renderer hint (seamark:light:#=<col>:<start>:<end>:<r>).\n"
          "   -i <node id> ... Set first id for numbering new nodes (default = -1).\n"
          "   -l <filename> .. Output errors to file <filename>. Use \"stderr\" for output to stderr.\n"
          "   -r <radius> .... Default radius (default = %.2f nm).\n"
          "   -S ............. Do not render sectors.\n"
          "   -U ............. Render a circle if a sector has neither start nor end angle (default = %d).\n\n",
          s, arc_max_, dir_arc_, arc_div_, sec_radius_, untagged_circle_);
}


void sort_sectors(struct sector *sec, int n)
{
   struct sector ss;
   int i, j;

   for (j = 1; j < n - 1; j++)
      for (i = 0; i < n - j; i++)
      {
         if (sec[i].mean > sec[i + 1].mean)
         {
            memcpy(&ss, &sec[i], sizeof(struct sector));
            memcpy(&sec[i], &sec[i + 1], sizeof(struct sector));
            memcpy(&sec[i + 1], &ss, sizeof(struct sector));
         }
      }
}


int main(int argc, char *argv[])
{
   FILE *f = NULL;
   hpx_ctrl_t *ctl;
   hpx_tag_t *tag;
   bstring_t b;
   int i, j, k;
   struct osm_node *nd;
   hpx_tree_t *tlist = NULL;
#define MAX_SEC 32
   struct sector sec[MAX_SEC];

   int n;

   while ((n = getopt(argc, argv, "a:b:chHi:l:d:r:SU")) != -1)
      switch (n)
      {
         case 'a':
            arc_max_ = atof(optarg);
            break;

         case 'b':
            dir_arc_ = atof(optarg);
            break;

         case 'c':
            gen_lc_ = 1;
            break;

         case 'l':
            if (!strcmp(optarg, "stderr"))
            {
               f = stderr;
            }
            else if ((f = fopen(optarg, "w")) == NULL)
               fprintf(stderr, "*** Cannot open file '%s': %s\n", optarg, strerror(errno)),
                  exit(EXIT_FAILURE);
            log_set_stream(f);
            log_msg("\n# Smfilter log file. Numbers in square brackets show line numbers of\n# input/output file. The error is always in the node/way before\n# the printed line number.");
            break;

         case 'h':
            usage(argv[0]);
            exit(1);

         case 'H':
            parse_rhint_ = 1;
            break;

         case 'i':
            set_id(atol(optarg));
            break;

         case 'd':
            arc_div_ = atof(optarg);
            break;

         case 'r':
            sec_radius_ = atof(optarg);
            break;

         case 'S':
            gen_sec_ = 0;
            break;

         case 'U':
            untagged_circle_ = 1;
            break;
      }

   if ((arc_div_ <= 0) || (sec_radius_ <= 0) || (dir_arc_ <= 0))
      fprintf(stderr, "*** illegal parameters!\n"), exit(EXIT_FAILURE);

   if ((ctl = hpx_init_simple()) == NULL)
      perror("hpx_init_simple"), exit(EXIT_FAILURE);
   if ((nd = malloc_node()) == NULL)
      perror("malloc_node"), exit(EXIT_FAILURE);

   if (hpx_tree_resize(&tlist, 0) == -1)
      perror("hpx_tree_resize"), exit(EXIT_FAILURE);
   if ((tlist->tag = hpx_tm_create(16)) == NULL)
      perror("hpx_tm_create"), exit(EXIT_FAILURE);

   tlist->nsub = 0;
   tag = tlist->tag;
   nd->type = OSM_NA;

   while (hpx_get_elem(ctl, &b, NULL, &tag->line) > 0)
   {
      if (!hpx_process_elem(b, tag))
      {
         hpx_fprintf_tag(stdout, tag);
         oline_++;
         if (!bs_cmp(tag->tag, "node"))
         {
            if (tag->type == HPX_OPEN)
            {
               nd->type = OSM_NODE;
               proc_osm_node(tag, nd);
               if (tlist->nsub >= tlist->msub)
               {
                  if (hpx_tree_resize(&tlist, 1) == -1)
                     perror("hpx_tree_resize"), exit(EXIT_FAILURE);
                  if (hpx_tree_resize(&tlist->subtag[tlist->nsub], 0) == -1)
                     perror("hpx_tree_resize"), exit(EXIT_FAILURE);
                  if ((tlist->subtag[tlist->nsub]->tag = hpx_tm_create(16)) == NULL)
                     perror("hpx_tm_create"), exit(EXIT_FAILURE);
               }
               tlist->subtag[tlist->nsub]->nsub = 0;
               tag = tlist->subtag[tlist->nsub]->tag;
            }
            else if (tag->type == HPX_CLOSE)
            {
               if (match_node(tlist, &b))
               {
                  // init sector list
                  for (i = 0; i < MAX_SEC; i++)
                     init_sector(&sec[i]);

                  i = get_sectors(tlist, sec, MAX_SEC);
                  if (gen_lc_)
                     pchar(nd, &sec[0]);
                  if (i)
                  {
                     for (i = 0, n = 0; i < MAX_SEC; i++)
                     {
                        // check all parsed sectors for its validity and remove
                        // illegal sectors
                        if (sec[i].used)
                        {
                           // Skip 0 degree sector if it is a directional
                           // light. Such definitions are incorrect and have
                           // been accidently imported with the LoL import.
                           if (i && (sec[i].start == sec[i].end) && (sec[i].start == sec[0].dir))
                           {
                              log_msg("deprecated feature: %d:sector_start == %d:sector_end == orientation (node %ld)", sec[i].nr, sec[i].nr, nd->id);
                              sec[i].used = 0;
                              continue;
                           }

                           if ((!isnan(sec[i].dir) && (sec[i].cat != CAT_DIR)) ||
                                (isnan(sec[i].dir) && (sec[i].cat == CAT_DIR)))
                           {
                              log_msg("sector %d has incomplete definition of directional light (node %ld)", sec[i].nr, nd->id);
                              sec[i].dir = NAN;
                              sec[i].cat = 0;
                              sec[i].used = 0;
                              continue;
                           }
                           if (isnan(sec[i].start) && isnan(sec[i].end))
                           {
                              if (sec[i].cat == CAT_DIR)
                              {
                                 sec[i].start = sec[i].end = sec[i].dir;
                              }
                              else if (untagged_circle_)
                              {
                                 sec[i].start = 0.0;
                                 sec[i].end = 360.0;
                              }
                              else
                              {
                                 log_msg("sector %d of node %ld seems to lack start/end angle", sec[i].nr, nd->id);
                                 sec[i].used = 0;
                                 continue;
                              }
                           }
                           else if (isnan(sec[i].start) || isnan(sec[i].end))
                           {
                              log_msg("sector %d of node %ld has either no start or no end angle!", sec[i].nr, nd->id);
                              sec[i].used = 0;
                              continue;
                           }

                           if (sec[i].start > sec[i].end)
                              sec[i].end += 360;

                           // increase counter for valid sectors
                           n++;
                        } // if (sec[i].used)
                     } // for (i = 0; i < MAX_SEC; i++)

                     // remove all unused (or invalid) sectors
                     for (i = 0, j = 0; i < MAX_SEC && j < n; i++, j++)
                     {
                        if (sec[i].used)
                        {
                           sec[i].mean = (sec[i].start + sec[i].end) / 2;
                           continue;
                        }
                        memcpy(&sec[i], &sec[i + 1], sizeof(struct sector) * (MAX_SEC - i - 1));
                        init_sector(&sec[MAX_SEC - 1]);
                        i--;
                        j--;
                     }
 
                     // sort sectors ascending on der mean angle
                     sort_sectors(&sec[0], n);

                     sec[n - 1].espace = sec[0].sspace = sec[0].start - sec[n - 1].end;
                     for (i = 0; i < n - 1; i++)
                        sec[i].espace = sec[i + 1].sspace = sec[i + 1].start - sec[i].end;
                     /*
                     if (sec[n - 1].end - 360 > sec[0].start)
                        sec[n - 1].end = sec[0].start = (sec[n - 1].end  - 360 + sec[0].start) / 2;
                     else
                        sec[n - 1].espace = sec[0].sspace = sec[0].start - sec[n - 1].end + 360;

                     for (i = 0; i < n - 1; i++)
                     {
                        if (sec[i].end > sec[i + 1].start)
                           sec[i].end = sec[i + 1].start = (sec[i].end + sec[i + 1].start) / 2;
                        else
                           sec[i].espace = sec[i + 1].sspace = sec[i + 1].start - sec[i].end;
                     }
                     */

                     // render sectors
                     for (i = 0; i < MAX_SEC; i++)
                     {
                        if (sec[i].used && gen_sec_)
                        {
                           if (proc_sfrac(&sec[i]) == -1)
                           {
                              log_msg("negative angle definition is just allowed in last segment! (sector %d node %ld)", sec[i].nr, nd->id);
                              continue;
                           }
                           //printf("   <!-- [%d]: start = %.2f, end = %.2f, col = %d, r = %.2f, nr = %d -->\n",
                           //   i, sec[i].start, sec[i].end, sec[i].col, sec[i].r, sec[i].nr);
                           sector_calc2(nd, &sec[i], b);

                           if (sec[i].col[1] != -1)
                           {
                              sec[i].sf[0].startr = sec[i].sf[sec[i].fused - 1].endr = 0;
                              for (j = 0; j < 4; j++)
                              {
                                 for (k = 0; k < sec[i].fused; k++)
                                    sec[i].sf[k].r -= altr_[j];
                                 sec[i].al++;
                                 sector_calc2(nd, &sec[i], b);
                              }
                           }
                        }
                     } // for (i = 0; n && i < MAX_SEC; i++)
                  } // if (get_sectors(tlist, sec, MAX_SEC))
               } // if (match_node(tlist))

               tlist->nsub = 0;
               tag = tlist->tag;
               nd->type = OSM_NA;
            }
            continue;
         } //if (!bs_cmp(tag->tag, "node"))

         if (nd->type != OSM_NODE)
            continue;

         if (!bs_cmp(tag->tag, "tag"))
         {
            tlist->nsub++;
            if (tlist->nsub >= tlist->msub)
            {
               if (hpx_tree_resize(&tlist, 1) == -1)
                  perror("hpx_tree_resize"), exit(EXIT_FAILURE);
               if (hpx_tree_resize(&tlist->subtag[tlist->nsub], 0) == -1)
                  perror("hpx_tree_resize"), exit(EXIT_FAILURE);
               if ((tlist->subtag[tlist->nsub]->tag = hpx_tm_create(16)) == NULL)
                  perror("hpx_tm_create"), exit(EXIT_FAILURE);
            }
            tlist->subtag[tlist->nsub]->nsub = 0;
            tag = tlist->subtag[tlist->nsub]->tag;
         }
      }
   }

   if (f != NULL)
      fclose(f);

   hpx_tm_free(tag);
   hpx_free(ctl);
   free(nd);

   exit(EXIT_SUCCESS);
}


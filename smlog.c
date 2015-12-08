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

#include <stdio.h>
#include <stdarg.h>

#include "libhpxml.h"


extern int oline_;
static FILE *flog_ = NULL;


void log_set_stream(FILE *f)
{
   flog_ = f;
}


int log_msg(const char *fmt, ...)
{
   int n;
   va_list ap;

   if (flog_ == NULL)
      return 0;

   fprintf(flog_, "[%ld/%d] ", hpx_lineno(), oline_);
   va_start(ap, fmt);
   n = vfprintf(flog_, fmt, ap);
   va_end(ap);
   fprintf(flog_, "\n");

   return n;
}



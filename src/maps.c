/*
 * Representation of a map from one free module to another
 *
 * Copyright (C) 2003 Christian Nassau <nassau@nullhomotopie.de>
 *
 *  $Id$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include "maps.h"

int diffRealloc(diff *df, int newalloc) {
    diffsum *ndat;
    if (newalloc < df->num) { newalloc = df->num; }
    ndat = realloc(df->dat, newalloc*sizeof(diffsum));
    if (NULL == ndat) return 0;
    df->dat = ndat; 
    df->alloc = newalloc;    
    return 1;
}

void diffsumDestroy(diffsum *ds) {
    if (NULL != ds->dat) free(ds->dat);
    free(ds);
}

void diffDestroy(diff *df) {
    int i;
    for (i=df->num;i--;) diffsumDestroy(&(df->dat[i]));
    free(df->dat);
    free(df);
}

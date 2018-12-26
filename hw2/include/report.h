#ifndef REPORT_H
#define REPORT_h


#include <stdio.h>

void reportparams(FILE *fd, char *fn, Course *c);
void reportfreqs(FILE *fd, Stats *s);
void reportmoments(FILE *fd, Stats *s);
void reportscores(FILE *fd, Course *c, int nm);
void reportcomposites(FILE *fd, Course *c, int nm);
void reporthistos(FILE *fd, Course *c, Stats *s);
void reportquantiles(FILE *fd, Stats *s);
void reportquantilesummaries(FILE *fd, Stats *s);
void reporttabs(FILE *fd, Course *c);

void histo(FILE *fd, int bins[50], float min, float max, int cnt);

#endif

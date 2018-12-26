
/*
 * Type definitions for normalization functions.
 */
#ifndef NORMAL_H
#define NORMAL_H

void normalize(Course *);
float normal(double s, Classstats *csp, Sectionstats *ssp);
float linear(double s, double rm, double rd, double nm, double nd);
float scale(double s, double max, double scale);
float studentavg(Student *s, Atype t);
void composites(Course *c);

#endif
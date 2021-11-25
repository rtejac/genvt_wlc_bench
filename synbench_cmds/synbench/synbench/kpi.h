#ifndef __KPI_H__
#define __KPI_H__

#include "main.h"

int init_kpi_model(struct config *conf);
void evaluate_kpis(struct config *conf, double frameTime);
void cleanup_kpi_model();

#endif // __KPI_H__
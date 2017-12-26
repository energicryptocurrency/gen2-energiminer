/*
 * Plant.h
 *
 *  Created on: Dec 13, 2017
 *      Author: ranjeet
 */

#ifndef PLANT_H_
#define PLANT_H_

#include "energiminer/solution.h"

namespace energi
{
  class Plant
  {
  public:
    virtual ~Plant(){}
    virtual void submit(const Solution &m) const = 0;
  };
}


#endif /* PLANT_H_ */

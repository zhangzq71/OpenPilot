/**
 * mapManagerAbstract.cpp
 *
 * \date 10/03/2010
 * \author nmansard@laas.fr
 *
 *  \file mapManagerAbstract.cpp
 *
 *  ## Add a description here ##
 *
 * \ingroup rtslam
 */

#include <boost/shared_ptr.hpp>

#include "rtslam/rtSlam.hpp"
#include "rtslam/mapManager.hpp"
#include "rtslam/landmarkAbstract.hpp"
#include "rtslam/observationFactory.hpp"
#include "rtslam/observationAbstract.hpp"
#include "rtslam/dataManagerAbstract.hpp"

namespace jafar {
	namespace rtslam {
		using namespace std;

		observation_ptr_t MapManagerAbstract::createNewLandmark(
		    data_manager_ptr_t dmaOrigin) {
			landmark_ptr_t newLmk = createLandmarkInit();
			newLmk->setId();
			newLmk->linkToParentMapManager(shared_from_this());
			observation_ptr_t resObs;

			for (MapManagerAbstract::DataManagerList::iterator iterDMA =
			    dataManagerList().begin(); iterDMA != dataManagerList().end(); ++iterDMA) {
				data_manager_ptr_t dma = *iterDMA;
				observation_ptr_t newObs =
				    dma->observationFactory()->create(dma->sensorPtr(), newLmk);

				/* Insert the observation in the graph. */
				newObs->linkToParentDataManager(dma);
				newObs->linkToParentLandmark(newLmk);
				newObs->linkToSensor(dma->sensorPtr());
				newObs->linkToSensorSpecific(dma->sensorPtr());
				newObs->setId();

				/* Store for the return the obs corresponding to the dma origin. */
				if (dma == dmaOrigin) resObs = newObs;
			}
			return resObs;
		}

	  void MapManagerAbstract::unregisterLandmark(landmark_ptr_t lmkPtr,bool liberateFilter){
			// first unlink all observations
			for (LandmarkAbstract::ObservationList::iterator obsIter =
			    lmkPtr->observationList().begin(); obsIter != lmkPtr->observationList().end(); ++obsIter) {
				observation_ptr_t obsPtr = *obsIter;
				obsPtr->dataManagerPtr()->unregisterChild(obsPtr);
			}
			// liberate map space
			if( liberateFilter )
			  mapPtr()->liberateStates(lmkPtr->state.ia());
			// now unlink landmark
			ParentOf<LandmarkAbstract>::unregisterChild(lmkPtr);
		}


		void MapManagerAbstract::manage(void) {
			// foreach lmk
			for (LandmarkList::iterator lmkIter = landmarkList().begin(); lmkIter
			    != landmarkList().end(); ++lmkIter) {
				landmark_ptr_t lmkPtr = *lmkIter;
				if (lmkPtr->needToDie() )
				{
					lmkIter++;
					unregisterLandmark(lmkPtr);
					lmkIter--;
				} else if (lmkPtr->needToReparametrize() )
				{
					lmkIter++;
					reparametrizeLandmark(lmkPtr);
					lmkIter--;
				}
			}
		}



    void MapManagerAbstract::reparametrizeLandmark(landmark_ptr_t lmkinit) {
			//cout<<__PRETTY_FUNCTION__<<"(#"<<__LINE__<<"): " <<"" << endl;

			// unregister lmk
			unregisterLandmark(lmkinit, false);

			// Create a new landmark advanced instead of the previous init lmk.
			jblas::ind_array idxComp(sizeComplement());
			//cout << __PRETTY_FUNCTION__ << "about to create lmkcv." << endl;
			landmark_ptr_t lmkconv = createLandmarkConverged(lmkinit, idxComp);

			// link new landmark
			lmkconv->linkToParentMapManager(shared_from_this());

			// Algebra: 2a. compute the jac and 2b. update the filter.
			// a. Call reparametrize_func()
			const size_t size_init = lmkinit->mySize();
			const size_t size_conv = lmkconv->mySize();
			mat CONV_init(size_conv, size_init);
			vec sinit = lmkinit->state.x();
			vec sconv(size_conv);
			//cout << __PRETTY_FUNCTION__ << "about to call lmk->reparametrize_func()" << endl;
			lmkinit->reparametrize_func(sinit, sconv, CONV_init);

			// b. Call filter->reparametrize().
			//cout << __PRETTY_FUNCTION__ << "about to call filter->reparametrize()" << endl;
			lmkconv->state.x() = sconv;
			mapPtr()->filterPtr->reparametrize(mapPtr()->ia_used_states(), CONV_init,
			                                   lmkinit->state.ia(),
			                                   lmkconv->state.ia());

			// Transfer info from the old lmk to the new one.
			//cout << __PRETTY_FUNCTION__ << "about to transfer lmk info." << endl;
			lmkconv->transferInfoLmk(lmkinit);

			// Create the cv-lmk set of observations, one per sensor.
			for (LandmarkAbstract::ObservationList::iterator obsIter =
			    lmkinit->observationList().begin(); obsIter
			    != lmkinit->observationList().end(); ++obsIter) {
				observation_ptr_t obsinit = *obsIter;
				data_manager_ptr_t dma = obsinit->dataManagerPtr();
				sensor_ptr_t sen = obsinit->sensorPtr();

				//cout << __PRETTY_FUNCTION__ << "about to create new obs" << endl;
				observation_ptr_t obsconv = dma->observationFactory()->create(sen,
				                                                            lmkconv);
				obsconv->linkToParentDataManager(dma);
				obsconv->linkToParentLandmark(lmkconv);
				obsconv->linkToSensor(sen);
				obsconv->linkToSensorSpecific(sen);
				// transfer info to new obs
				obsconv->transferInfoObs(obsinit);
			}

			// liberate unused map space.
			mapPtr()->liberateStates(idxComp);

		}

	}
  ;
}
;

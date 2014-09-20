/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014                                                                    *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <openspace/scenegraph/spiceephemeris.h>

#include <openspace/util/spice.h>
#include <openspace/util/spicemanager.h>
#include <openspace/util/time.h>

namespace openspace {
    
SpiceEphemeris::SpiceEphemeris(const ghoul::Dictionary& dictionary): _targetName(""),
                                                                     _originName(""),
                                                                     _target(0),
                                                                     _origin(0),
                                                                     _position()
{
    dictionary.getValue("Body", _targetName);
    dictionary.getValue("Observer", _originName);
}
SpiceEphemeris::~SpiceEphemeris() {}

bool SpiceEphemeris::initialize() {
    
    if (_targetName != "" && _originName != "") {
        int bsuccess = 0;
        int osuccess = 0;
        Spice::ref().bod_NameToInt(_targetName, &_target, &bsuccess);
        Spice::ref().bod_NameToInt(_originName, &_origin, &osuccess);
        
        if (bsuccess && osuccess) {
            return true;
        }
    }
    
    return false;
}

const psc& SpiceEphemeris::position() const {
    return _position;
}

void SpiceEphemeris::update(RuntimeData* runtimeData) {
    double state[3];
   
	_currentEphemerisTime = runtimeData->getTime();
	
	glm::dvec3 position(0,0,0);

	double lightTime = 0.0;
	SpiceManager::ref().getTargetPosition(_targetName, _currentEphemerisTime, "GALACTIC", "LT+S", _originName, position, lightTime);

	/*
	std::cout << _targetName  << " (";
	std::cout << position[0] << ", ";
	std::cout << position[1] << ", ";
	std::cout << position[2] << ")";
	std::cout << std::endl;
	assert(_targetName != "JUPITER");
	*/
	_position = psc::CreatePowerScaledCoordinate(position.x, position.y, position.z);
	_position[3] -= 0.01;

	//_position[3] += 1;
	//_position[3] += 3;

}

} // namespace openspace
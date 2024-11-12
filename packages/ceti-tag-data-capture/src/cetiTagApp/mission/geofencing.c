#include <math.h>
typedef struct {
    float lat;
    float lon;
} Coord;

typedef struct {
    Coord position;
    float radius_km;
} GeofenceElement;

const Coord dive_dom = {
    .lat = 15.291171,
    .lon = -61.379647,
}

struct {
    int count;
    GeofenceElement description[];
} geofence = {
    .count = 1;
    .description = {
        .position = {
            .lat = 15.291171,
            .lon = -61.379647,
        },
        .radius_km = 2,
    }
}

/* Goals: 
    1) define geofencing descriptors
        e.g. NSEW of specific Lat/long
        e.g. Withing radius of point
    2) nmea parsing for current position and expected position
    3) searching of current position within geofence point cloud
    4) 

    Notes:

    struct geofencing_point{
        lat:
        lon:
        max_norm_km:???
    }

    struct geofence {
        lat: (min, max) # L_inf
        lon: (min, max) # L_inf
        cloud: [geofencing_point;?]
    }
*/

/*
    point
    if point is outside limits L_inf norm
        detatch
    
*/
#define EARTH_RADIUS_KM 6371.0
#define DTOR(degrees) ((degrees) * M_PI / 180.0)

/* convert 2 gps positions to a distance in km */
double global_distance(const Coord *x, const Coord *y) {
    double dLat = DTOR(x->lat - y->lat);
    double dLon = DTOR(x->lon - y->lon);

    double a = sin(dLat/2)*sin(dLat/2) + sin(dLon/2)*sin(dLon/2)*cos(DTOR(x->lat))*cos(DTOR(y->lat));
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    return EARTH_RADIUS_KM * c;

}

/* check if a point is within the geofence */
bool point_is_within_geofence(const Coord *position) {
    // iterate through all points
   for(int i = 0; i < geofence.count; i++) {
        GeofenceElement *i_geo = &geofence.description[i];
        double dist = global_distance(position, &i_geo->position);
        if (dist < radius) {
            return true;
        }
   }
   return false;
}

/* initialize geofence from description file */
void geofence_initialize(void) {
    // ToDo: read geofence from file
    // ToDo: 
}



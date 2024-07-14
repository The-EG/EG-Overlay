import requests
import datetime

def specializations(f):
    print('  /specializations')
    r = requests.get("https://api.guildwars2.com/v2/specializations?ids=all", headers={'X-Schema-Version': 'latest'})
    f.write('\n')
    f.write('-------------------------------------------------------------------------------\n')
    f.write('-- specializations\n')
    f.write('-------------------------------------------------------------------------------\n')

    if r.status_code >= 400:
        print(f"Got {r.status_code} while fetching specializations.")
        exit()

    f.write('gw2.specializations = {}\n')

    for spec in r.json():
        elite = "true" if spec["elite"] else "false"
        minor_traits = ','.join([str(x) for x in spec['minor_traits']])
        major_traits = ','.join([str(x) for x in spec['major_traits']])
        f.write(f'gw2.specializations[{spec["id"]}] = {{ ')
        f.write(f'name = "{spec["name"]}", ')
        f.write(f'profession = "{spec["profession"]}", ')
        f.write(f'elite = {elite}, ')
        f.write(f'minor_traits = {{ { minor_traits } }}, ')
        f.write(f'major_traits = {{ {major_traits} }}, ')
        f.write(f'icon = "{spec["icon"]}", ')
        f.write(f'background = "{spec["background"]}" ')
        f.write('}\n')

def continents_maps(f):
    print(' ')
    r = requests.get("https://api.guildwars2.com/v2/continents/1/floors/0", headers={'X-Schema-Version': 'latest'})
    f.write('\n')
    f.write('-------------------------------------------------------------------------------\n')
    f.write('-- maps (Tyria)\n')
    f.write('-------------------------------------------------------------------------------\n')

    if r.status_code >= 400:
        print(f"Got {r.status_code} while fetching continent data.")
        exit()

    f.write('gw2.maps = {}\n')

    data = r.json()

    for regionid in data['regions']:
        region = data['regions'][regionid]
        regionname = region['name']
        for mapid in region['maps']:
            map = region['maps'][mapid]
            map_rect = f'{{ {{ {map["map_rect"][0][0]}, {map["map_rect"][0][1]} }}, {{ {map["map_rect"][1][0]}, {map["map_rect"][1][1]} }} }}'
            continent_rect = f'{{ {{ {map["continent_rect"][0][0]}, {map["continent_rect"][0][1]} }}, {{ {map["continent_rect"][1][0]}, {map["continent_rect"][1][1]} }} }}'

            pois = {poiid: map['points_of_interest'][poiid] for poiid in map['points_of_interest'] if map['points_of_interest'][poiid]['type']=='landmark'}
            waypoints = {poiid: map['points_of_interest'][poiid] for poiid in map['points_of_interest'] if map['points_of_interest'][poiid]['type']=='waypoint'}
            vistas = {poiid: map['points_of_interest'][poiid] for poiid in map['points_of_interest'] if map['points_of_interest'][poiid]['type']=='landmark'}

            f.write(f'gw2.maps[{mapid}] = {{\n')
            f.write(f'    name = "{map['name']}",\n')
            f.write(f'    min_level = {map["min_level"]},\n')
            f.write(f'    max_level = {map["max_level"]},\n')
            f.write(f'    default_floor = {map["default_floor"]},\n')
            f.write(f'    region_id = {regionid},\n')
            f.write(f'    region_name = {regionname},\n')
            f.write(f'    continent_id = 1,\n')
            f.write(f'    continent_name = "Tyria",\n')
            f.write(f'    map_rect = {map_rect},\n')
            f.write(f'    continent_rect = {continent_rect}\n')
            f.write( '    pois = {\n')
            for poiid in pois:                
                f.write(f'        {poiid} = {{\n')
                f.write(f'            name = {pois[poiid]['name']},\n')
                f.write( '},')
            f.write( '    },\n')
            f.write( '}\n')

def maps(f):
    print('  /maps')
    r = requests.get("https://api.guildwars2.com/v2/maps?ids=all", headers={'X-Schema-Version': 'latest'})
    f.write('\n')
    f.write('-------------------------------------------------------------------------------\n')
    f.write('-- /v2/maps\n')
    f.write('-------------------------------------------------------------------------------\n')

    if r.status_code >= 400:
        print(f"Got {r.status_code} while fetching maps.")
        exit()

    f.write('gw2.maps = {}\n')

    for map in r.json():
        floors = ', '.join([str(x) for x in map['floors']])
        map_rect = f'{{ {{ {map["map_rect"][0][0]}, {map["map_rect"][0][1]} }}, {{ {map["map_rect"][1][0]}, {map["map_rect"][1][1]} }} }}'
        continent_rect = f'{{ {{ {map["continent_rect"][0][0]}, {map["continent_rect"][0][1]} }}, {{ {map["continent_rect"][1][0]}, {map["continent_rect"][1][1]} }} }}'

        # some maps don't have a region
        region_id = map['region_id'] if 'region_id' in map else 'nil'
        region_name = f'"{map["region_name"]}"' if 'region_name' in map else 'nil'
        
        # or continent
        continent_id = map['continent_id'] if 'continent_id' in map else 'nil'
        continent_name = f'"{map["continent_name"]}"' if 'continent_name' in map else 'nil'

        map_name = map["name"].replace('"', '\\"')

        f.write(f'gw2.maps[{map["id"]}] = {{\n')
        f.write(f'    name = "{map_name}",\n')
        f.write(f'    min_level = {map["min_level"]},\n')
        f.write(f'    max_level = {map["max_level"]},\n')
        f.write(f'    default_floor = {map["default_floor"]},\n')
        f.write(f'    region_id = {region_id}, ')
        f.write(f'region_name = {region_name}, ')
        f.write(f'continent_id = {continent_id}, ')
        f.write(f'continent_name = {continent_name}, ')
        f.write(f'map_rect = {map_rect}, ')
        f.write(f'continent_rect = {continent_rect} ')
        f.write('}\n')


if __name__=='__main__':
    print("writing src/lua/gw2/static.lua...")

    f = open('src/lua/gw2/static.lua', 'w', encoding='utf-8')

    f.write('-------------------------------------------------------------------------------\n')
    f.write('-- This file is automatically generated from scripts/generate_lua_gw2_static.py\n')
    f.write('-- DO NOT EDIT IT MANUALLY\n')
    f.write(f'-- Generated at: {str(datetime.datetime.now())}\n')
    f.write('-------------------------------------------------------------------------------\n')
    f.write('\n')

    f.write('local gw2 = {}\n')

    specializations(f)
    maps(f)

    f.write('\nreturn gw2\n')

    f.close()
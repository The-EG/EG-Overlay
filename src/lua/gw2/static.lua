--[[ RST
gw2.static
==========

.. lua:module:: gw2.static

.. code-block:: lua

    local static = require 'gw2.static'

]]--

local overlay = require 'eg-overlay'
local sqlite = require 'sqlite'
local logger = require 'logger'
local overlay = require 'eg-overlay'

local api = require 'gw2.api'

local static = {}

static.log = logger.logger:new('gw2.static')

--[[ RST
Attributes
----------

.. lua:data:: db

    A :lua:class:`sqlite` database housing all loaded GW2 API data.

    .. versionhistory::
        :0.0.1: Added

]]--

-- cheat a bit and create the db on load
-- this way it at least exists before the startup event runs
static.db = sqlite.open(overlay.data_folder('gw2') .. 'static.db')

--[[ RST
Database Tables
---------------

.. overlay:database:: gw2static
]]--

--[[ RST
.. overlay:dbtable:: achievements

    Achievements

    **Columns**

    =========== ======= ================================================
    Name        Type    Description
    =========== ======= ================================================
    id          INTEGER Achievement ID, matches the GW2 API ID
    icon        TEXT    Render service URL
    name        TEXT    Achievement name
    description TEXT    
    requirement TEXT    Requirement as listed in game
    locked_text TEXT    Description prior to unlocking
    type        TEXT    Either 'Default' or 'ItemSet'
    point_cap   INTEGER Maximum number of AP for repeatable achievements
    =========== ======= ================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_achievements_table_sql = [[
CREATE TABLE IF NOT EXISTS achievements (
    id INTEGER PRIMARY KEY NOT NULL,
    icon TEXT,
    name TEXT NOT NULL,
    description TEXT,
    requirement TEXT,
    locked_text TEXT,
    type TEXT,
    point_cap INTEGER
)
]]

local achievements_insert_sql = [[
INSERT INTO
achievements (id, icon, name, description, requirement, locked_text, type, point_cap)
VALUES (:id, :icon, :name, :description, :requirement, :locked_text, :type, :point_cap)
]]

--[[ RST
.. overlay:dbtable:: achievement_flags

    Achievement Flags

    Possible values:

    ==================== ===========================================
    Value                Description
    ==================== ===========================================
    Pvp                  Can only progress achievement in PvP or WvW
    CategoryDisplay      Meta achievement
    MoveToTop
    IgnoreNearlyComplete Not shown in game 'nearly complete' list
    Repeatable           Can be repeated multiple times
    Hidden
    RequiresUnlock
    RepairOnLogin
    Daily
    Weekly
    Monthly
    Permanent
    ==================== ===========================================

    **Columns**

    =========== ======= ======================
    Name        Type    Description
    =========== ======= ======================
    achievement INTEGER Achievement ID
    flag        TEXT    One of the flags above
    =========== ======= ======================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_achievement_flags_table_sql = [[
CREATE TABLE IF NOT EXISTS achievement_flags (
    achievement INTEGER NOT NULL REFERENCES achievements (id) ON DELETE CASCADE,
    flag TEXT NOT NULL,
    PRIMARY KEY (achievement, flag)
)
]]

local achievement_flags_insert_sql = [[
INSERT INTO
achievement_flags (achievement, flag)
VALUES (:achievement, :flag)
]]

--[[ RST
.. overlay:dbtable:: achievement_tiers

    Achievement Tiers

    **Columns**

    =========== ======= ===================================================
    Name        Type    Description
    =========== ======= ===================================================
    achievement INTEGER Achievement ID
    count       INTEGER Number of items that must be completed in this tier
    points      INTEGER AP awarded for this tier
    sequence    INTEGER Sequence order of this tier
    =========== ======= ===================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_achievement_tiers_table_sql = [[
CREATE TABLE IF NOT EXISTS achievement_tiers (
    achievement INTEGER NOT NULL REFERENCES achievements (id) ON DELETE CASCADE,
    count INTEGER NOT NULL,
    points INTEGER,
    sequence INTEGER NOT NULL,
    PRIMARY KEY (achievement, sequence)
)
]]

local achievement_tiers_insert_sql = [[
INSERT INTO
achievement_tiers (achievement, count, points, sequence)
VALUES (:achievement, :count, :points, :sequence)
]]

--[[ RST
.. overlay:dbtable:: achievement_prerequisites

    Achievement Prerequisites

    **Columns**

    ============ ======= ======================================================================================
    Name         Type    Description
    ============ ======= ======================================================================================
    achievement  INTEGER Achievement ID
    prerequisite INTEGER A different achievement ID that must be completed before this achievement is available
    ============ ======= ======================================================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_achievement_prerequisites_table_sql = [[
CREATE TABLE IF NOT EXISTS achievement_prerequisites (
    achievement INTEGER NOT NULL REFERENCES achievements (id) ON DELETE CASCADE,
    prerequisite INTEGER NOT NULL,
    PRIMARY KEY (achievement, prerequisite)
)
]]

local achievement_prerequisites_insert_sql = [[
INSERT INTO
achievement_prerequisites (achievement, prerequisite)
VALUES (:achievement, :prerequisite)
]]

--[[ RST
.. overlay:dbtable:: achievement_rewards

    Achievement Rewards

    **Columns**

    =========== ======= ===============================================================
    Name        Type    Description
    =========== ======= ===============================================================
    achievement INTEGER Achievement ID
    type        TEXT    ``'Coins'``, ``'Item'``, ``'Mastery'``, or ``'Title'``
    count       INTEGER The number of ``'Coins'`` or ``'Item'`` rewarded
    id          INTEGER The ``'Title'``, ``'Mastery'`` Point, or ``'Item'`` id rewarded
    region      TEXT    The ``'Mastery'`` Point region
    =========== ======= ===============================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_achievement_rewards_table_sql = [[
CREATE TABLE IF NOT EXISTS achievement_rewards (
    achievement INTEGER NOT NULL REFERENCES achievements (id) ON DELETE CASCADE,
    type TEXT NOT NULL,
    count INTEGER,
    id INTEGER,
    region TEXT
)
]]

local achievement_rewards_insert_sql = [[
INSERT INTO
achievement_rewards (achievement, type, count, id, region)
VALUES (:achievement, :type, :count, :id, :region)
]]

--[[ RST
.. overlay:dbtable:: achievement_bits

    Achievement Bits

    **Columns**

    =========== ======= ====================================================
    Name        Type    Description
    =========== ======= ====================================================
    achievement INTEGER Achievement ID
    sequence    INTEGER
    type        TEXT    ``'Text'``, ``'Item'``, ``'Minipet'``, or ``'Skin'``
    id          INTEGER ID of the ``'Item'``, ``'Minipet'``, or ``'Skin'``
    text        TEXT    Text description if type = ``'Text'``
    =========== ======= ====================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_achievement_bits_table_sql = [[
CREATE TABLE IF NOT EXISTS achievement_bits (
    achievement INTEGER NOT NULL REFERENCES achievements (id) ON DELETE CASCADE,
    sequence INTEGER NOT NULL,
    type TEXT,
    id INTEGER,
    "text" TEXT,
    PRIMARY KEY (achievement, sequence)
)
]]

local achievement_bits_insert_sql = [[
INSERT INTO
achievement_bits (achievement, sequence, type, id, "text")
VALUES (:achievement, :sequence, :type, :id, :text)
]]

--[[ RST
.. overlay:dbtable:: specializations

    Specializations

    **Columns**

    ============ ======= ==========================================================================
    Name         Type    Description
    ============ ======= ==========================================================================
    id           INTEGER Specialization ID, matches the ID returned by mumble-link and the GW2 API.
    name         TEXT    Specialization name.
    profession   TEXT
    elite        BOOL    Indicates if this is an elite specialization or not.
    weapon_trait INTEGER 
    icon         TEXT    Render service URL.
    background   TEXT    Render service URL.
    ============ ======= ==========================================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_specializations_table_sql = [[
CREATE TABLE IF NOT EXISTS specializations (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    profession TEXT NOT NULL,
    elite BOOL NOT NULL,
    weapon_trait INTEGER,
    icon TEXT NOT NULL,
    background TEXT NOT NULL
)
]]

local specializations_insert_sql = [[
INSERT INTO
specializations (id, name, profession, elite, weapon_trait, icon, background)
VALUES (:id, :name, :profession, :elite, :weapon_trait, :icon, :background)
]]

--[[ RST
.. overlay:dbtable:: specialization_traits

    Specialization Traits

    **Columns**

    ============== ======= =============================
    Name           Type    Description
    ============== ======= =============================
    id             INTEGER Trait ID
    specialization INTEGER Specialization ID
    major          BOOL    TRUE if this is a major trait
    ============== ======= =============================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_spec_traits_table_sql = [[
CREATE TABLE IF NOT EXISTS specialization_traits (
    id INTEGER NOT NULL,
    specialization INTEGER NOT NULL REFERENCES specializations (id) ON DELETE CASCADE,
    major BOOL,
    PRIMARY KEY (id, specialization)
)
]]

local spec_traits_insert_sql = [[
INSERT INTO
specialization_traits (id, specialization, major)
VALUES (:id, :specialization, :major)
]]


--[[ RST
.. overlay:dbtable:: continents

    GW2 Continents
    
    **Columns**
    
    ================ ======= ===============================================================
    Name             Type    Description
    ================ ======= ===============================================================
    id               INTEGER Continent ID, matches the ID returned by the GW2 API.
    name             TEXT    Continent name.
    continent_width  REAL    The width portion of `continent_dims` returned by the GW2 API.
    continent_height REAL    The height portion of `continent_dims` returned by the GW2 API.
    min_zoom         INTEGER  
    max_zoom         INTEGER
    ================ ======= ===============================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_continent_table_sql = [[
CREATE TABLE IF NOT EXISTS continents (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    continent_width REAL NOT NULL,
    continent_height REAL NOT NULL,
    min_zoom INTEGER NOT NULL,
    max_zoom INTEGER NOT NULL,
    floors TEXT
)
]]

local continent_insert_sql = [[
INSERT INTO
continents (id, name, continent_width, continent_height, min_zoom, max_zoom, floors)
VALUES (:id, :name, :continent_width, :continent_height, :min_zoom, :max_zoom, :floors)
]]

--[[ RST
.. overlay:dbtable:: regions

    GW2 Regions

    **Columns**

    ===================== ======= ===============================================================
    Name                  Type    Description
    ===================== ======= ===============================================================
    id                    INTEGER Region ID, matches the region ID returned by the GW2 API.
    name                  TEXT    Region name.
    continent             INTEGER The continent ID of this region.
    continent_rect_left   REAL    The left portion of `continent_rect` returned by the GW2 API.
    continent_rect_right  REAL    The right portion of `continent_rect` returned by the GW2 API.
    continent_rect_top    REAL    The top portion of `continent_rect` returned by the GW2 API.
    continent_rect_bottom REAL    The bottom portion of `continent_rect` returned by the GW2 API.
    ===================== ======= ===============================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_region_table_sql = [[
CREATE TABLE IF NOT EXISTS regions (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    continent_rect_left REAL NOT NULL,
    continent_rect_right REAL NOT NULL,
    continent_rect_top REAL NOT NULL,
    continent_rect_bottom REAL NOT NULL,
    continent INTEGER NOT NULL REFERENCES continents (id) ON DELETE CASCADE
)
]]

local region_insert_sql = [[
INSERT INTO 
regions (id, name, continent_rect_left, continent_rect_right, continent_rect_top, continent_rect_bottom, continent)
VALUES (:id, :name, :continent_rect_left, :continent_rect_right, :continent_rect_top, :continent_rect_bottom, :continent)
]]


--[[ RST
.. overlay:dbtable:: maps

    GW2 Maps

    **Columns**

    ===================== ======= =================================================================
    Name                  Type    Description
    ===================== ======= =================================================================
    id                    INTEGER Map ID, matches the id returned from mumble-link and the GW2 API.
    name                  TEXT    Map name.
    region                INTEGER Region ID.
    min_level             INTEGER
    max_level             INTEGER
    default_flooor        INTEGER
    label_x               REAL
    label_y               REAL
    map_rect_left         REAL    The left portion of `map_rect` returned by the GW2 API.
    map_rect_right        REAL    The right portion of `map_rect` returned by the GW2 API.
    map_rect_top          REAL    The top portion of `map_rect` returned by the GW2 API.
    map_rect_bottom       REAL    The bottom portion of `map_rect` returned by the GW2 API.
    continent_rect_left   REAL    The left portion of `continent_rect` returned by the GW2 API.
    continent_rect_right  REAL    The right portion of `continent_rect` returned by the GW2 API.
    continent_rect_top    REAL    The top portion of `continent_rect` returned by the GW2 API.
    continent_rect_bottom REAL    The bottom portion of `continent_rect` returned by the GW2 API.
    ===================== ======= =================================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_map_table_sql = [[
CREATE TABLE IF NOT EXISTS maps (
    id INTEGER PRIMARY KEY NOT NULL,
    name TEXT NOT NULL,
    min_level INTEGER NOT NULL,
    max_level INTEGER NOT NULL,
    default_floor INTEGER NOT NULL,
    label_x REAL,
    label_y REAL,
    map_rect_left REAL NOT NULL,
    map_rect_right REAL NOT NULL,
    map_rect_top REAL NOT NULL,
    map_rect_bottom REAL NOT NULL,
    continent_rect_left REAL NOT NULL,
    continent_rect_right REAL NOT NULL,
    continent_rect_top REAL NOT NULL,
    continent_rect_bottom REAL NOT NULL,
    region INTEGER NOT NULL REFERENCES regions (id) ON DELETE CASCADE
)
]]

local map_insert_sql = [[
INSERT INTO
maps (id, name, min_level, max_level, default_floor, label_x, label_y,
      map_rect_left, map_rect_right, map_rect_top, map_rect_bottom,
      continent_rect_left, continent_rect_right, continent_rect_top, continent_rect_bottom,
      region)
VALUES (:id, :name, :min_level, :max_level, :default_floor, :label_x, :label_y,
        :map_rect_left, :map_rect_right, :map_rect_top, :map_rect_bottom,
        :continent_rect_left, :continent_rect_right, :continent_rect_top, :continent_rect_bottom,
        :region)
]]

--[[ RST
.. overlay:dbtable:: pois

    GW2 points of interest. This includes waypoints, vistas, and actual POIs.

    **Columns**

    ========= ======= ===========================================================================================
    Name      Type    Description
    ========= ======= ===========================================================================================
    id        INTEGER
    map       INTEGER Map ID
    name      TEXT    POI name. May be NULL.
    type      TEXT    `landmark` = POI, `vista` = vista, `waypoint` = waypoint, `unlock` = other (dungeons, etc.)
    floor     INTEGER
    x         REAL    X coordinate of the POI in continent coordinates.
    y         REAL    Y coordinate of the POI in continent coordinates.
    chat_link TEXT
    icon      TEXT    Render service URL
    ========= ======= ===========================================================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_poi_table_sql = [[
CREATE TABLE IF NOT EXISTS pois (
    id INTEGER NOT NULL,
    map INTEGER NOT NULL REFERENCES maps (id) ON DELETE CASCADE,
    name TEXT,
    type TEXT NOT NULL,
    floor INTEGER,
    x REAL,
    y REAL,
    chat_link TEXT,
    icon TEXT,
    PRIMARY KEY (id, map)
);
CREATE INDEX IF NOT EXISTS idx_pois_type ON pois (type);
]]

local poi_insert_sql = [[
INSERT INTO
pois (id, name, type, floor, x, y, chat_link, icon, map)
VALUES (:id, :name, :type, :floor, :x, :y, :chat_link, :icon, :map)
]]

--[[ RST
.. overlay:dbtable:: tasks

    GW2 Renown Hearts

    **Columns**

    ========= ======= ==========================================================
    Name      Type    Description
    ========= ======= ==========================================================
    id        INTEGER
    map       INTEGER Map ID.
    objective TEXT    The text description of the renown heart.
    level     INTEGER
    x         REAL    X coordinate of the renown heart in continent coordinates.
    y         REAL    Y coordinate of the renown heart in continent coordinates.
    chat_link TEXT
    ========= ======= ==========================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_tasks_table_sql = [[
CREATE TABLE IF NOT EXISTS tasks (
    id INTEGER PRIMARY KEY NOT NULL,
    objective TEXT,
    level INTEGER,
    x REAL,
    y REAL,
    chat_link TEXT,
    map NOT NULL REFERENCES maps (id) ON DELETE CASCADE
)
]]

local task_insert_sql = [[
INSERT INTO
tasks (id, objective, level, x, y, chat_link, map)
VALUES (:id, :objective, :level, :x, :y, :chat_link, :map)
]]

--[[ RST
.. overlay:dbtable:: taskbounds

    Renown Heart Boundary Coordinates

    **Columns**

    ==== ======= =======================
    Name Type    Description
    ==== ======= =======================
    id   INTEGER
    seq  INTEGER Sequence ID.
    task INTEGER Renown Heart (task) ID.
    x    REAL
    y    REAL
    ==== ======= =======================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_taskbounds_table_sql = [[
CREATE TABLE IF NOT EXISTS taskbounds (
    id INTEGER PRIMARY KEY NOT NULL,
    seq INTEGER NOT NULL,
    task INTEGER NOT NULL REFERENCES tasks (id) ON DELETE CASCADE,
    x REAL NOT NULL,
    y REAL NOT NULL,
    UNIQUE (task, seq)
)
]]

local taskbounds_insert_sql = [[
INSERT INTO
taskbounds (seq, task, x, y)
VALUES (:seq, :task, :x, :y)
]]

--[[ RST
.. overlay:dbtable:: skillchallenges

    Hero Point Locations

    **Columns**

    ==== ======= ===========
    Name Type    Description
    ==== ======= ===========
    id   TEXT
    map  INTEGER Map ID. 
    x    REAL
    y    REAL
    ==== ======= ===========

    .. versionhistory::
        :0.0.1: Added
]]--
local create_skillchallenge_table_sql = [[
CREATE TABLE IF NOT EXISTS skillchallenges (
    id TEXT PRIMARY KEY,
    x REAL,
    y REAL,
    map INTEGER NOT NULL REFERENCES maps (id) ON DELETE CASCADE
)
]]

local skillchallenge_insert_sql = [[
INSERT INTO
skillchallenges (id, x, y, map)
VALUES (:id, :x, :y, :map)
]]

--[[ RST
.. overlay:dbtable:: sectors

    Map sectors

    **Columns**

    ===== ======= ================
    Name  Type    Description
    ===== ======= ================
    id    INTEGER
    map   INTEGER Map ID.
    name  TEXT
    level
    x     REAL    Center X location
    y     REAL    Center Y location
    ===== ======= ================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_sector_table_sql = [[
CREATE TABLE IF NOT EXISTS sectors (
    id INTEGER NOT NULL,
    map INTEGER NOT NULL REFERENCES maps (id) ON DELETE CASCADE,
    name TEXT,
    level NOT NULL,
    x REAL,
    y REAL,    
    PRIMARY KEY (id, map)
)
]]

local sector_insert_sql = [[
INSERT INTO
sectors (id, name, level, x, y, map)
VALUES (:id, :name, :level, :x, :y, :map)
]]

--[[ RST
.. overlay:dbtable:: sectorbounds

    Map sector boundary coordinates

    **Columns**

    ====== ======= ===============
    Name   Type    Description
    ====== ======= ===============
    id     INTEGER
    seq    INTEGER Sequence number
    sector INTEGER Sector ID
    map    INTEGER Map ID
    x      REAL
    y      REAL
    ====== ======= ===============

    .. versionhistory::
        :0.0.1: Added
]]--
local create_sectorbounds_table_sql = [[
CREATE TABLE IF NOT EXISTS sectorbounds (
    id INTEGER PRIMARY KEY,
    seq INTEGER NOT NULL,
    sector INTEGER NOT NULL,
    map INTEGER NOT NULL,
    x REAL,
    y REAL,
    UNIQUE (seq, sector, map),
    FOREIGN KEY (sector, map) REFERENCES sectors (id, map) ON DELETE CASCADE
)
]]

local sectorbounds_insert_sql = [[
INSERT INTO
sectorbounds (seq, sector, map, x, y)
VALUES (:seq, :sector, :map, :x, :y)
]]

--[[ RST
.. overlay:dbtable:: adventures

    Adventure locations

    **Columns**

    =========== ======= ===============
    Name        Type    Description
    =========== ======= ===============
    id          TEXT    Adventure GUID.
    map         INTEGER Map ID.
    name        TEXT
    description TEXT
    x           REAL
    y           REAL
    =========== ======= ===============

    .. versionhistory::
        :0.0.1: Added
]]--
local create_adventures_table_sql = [[
CREATE TABLE IF NOT EXISTS adventures (
    id TEXT NOT NULL,
    name TEXT NOT NULL,
    x REAL,
    y REAL,
    description TEXT,
    map INTEGER NOT NULL REFERENCES maps (id) ON DELETE CASCADE
)
]]

local adventures_insert_sql = [[
INSERT INTO
adventures (id, name, x, y, description, map)
VALUES (:id, :name, :x, :y, :description, :map)
]]

--[[ RST
.. overlay:dbtable:: masterypoints

    Mastery Point locations

    **Columns**

    ====== ======= =======================================================================================================
    Name   Type    Description
    ====== ======= =======================================================================================================
    id     INTEGER
    map    INTEGER Map ID
    region TEXT    The region/type of the mastery point. One of: `Tyria`, `Maguuma`, `Desert`, `Tundra`, `Jade`, or `Sky`.
    x      REAL
    Y      REAL
    ====== ======= =======================================================================================================

    .. versionhistory::
        :0.0.1: Added
]]--
local create_masterypoints_table_sql = [[
CREATE TABLE IF NOT EXISTS masterypoints (
    id INTEGER PRIMARY KEY,
    x REAL,
    y REAL,
    region TEXT,
    map INTEGER NOT NULL REFERENCES maps (id) ON DELETE CASCADE
)
]]

local masterypoints_insert_sql = [[
INSERT INTO
masterypoints (id, x, y, region, map)
VALUES (:id, :x, :y, :region, :map)
]]

static.db:execute(create_achievements_table_sql)
static.db:execute(create_achievement_tiers_table_sql)
static.db:execute(create_achievement_rewards_table_sql)
static.db:execute(create_achievement_prerequisites_table_sql)
static.db:execute(create_achievement_flags_table_sql)
static.db:execute(create_achievement_bits_table_sql)

static.db:execute(create_specializations_table_sql)
static.db:execute(create_spec_traits_table_sql)

static.db:execute(create_continent_table_sql)
static.db:execute(create_region_table_sql)
static.db:execute(create_map_table_sql)
static.db:execute(create_poi_table_sql)
static.db:execute(create_tasks_table_sql)
static.db:execute(create_taskbounds_table_sql)
static.db:execute(create_skillchallenge_table_sql)
static.db:execute(create_sector_table_sql)
static.db:execute(create_sectorbounds_table_sql)
static.db:execute(create_adventures_table_sql)
static.db:execute(create_masterypoints_table_sql)

local function runupdateachievements()
    static.log:info('Updating achievement data...')

    local r,achievement_ids = api.get('achievements', nil, nil, nil, nil, false)

    if not r then
        error("Couldn't fetch achievement ids")
    end

    local idbuckets = {}
    local curbucket = {}

    for i, id in ipairs(achievement_ids) do
        if #curbucket >= 100 then
            table.insert(idbuckets, curbucket)
            curbucket = {}
        end

        table.insert(curbucket, id)
    end
    if #curbucket > 0 then table.insert(idbuckets, curbucket) end

    static.log:info('%d achievements, fetching in %d groups of 100', #achievement_ids, #idbuckets)

    static.db:execute('DELETE FROM achievements')
    static.db:execute('DELETE FROM achievement_flags')
    static.db:execute('DELETE FROM achievement_tiers')
    static.db:execute('DELETE FROM achievement_prerequisites')
    static.db:execute('DELETE FROM achievement_rewards')
    static.db:execute('DELETE FROM achievement_bits')

    local achievementins = static.db:prepare(achievements_insert_sql)
    local flagsins = static.db:prepare(achievement_flags_insert_sql)
    local tiersins = static.db:prepare(achievement_tiers_insert_sql)
    local prereqsins = static.db:prepare(achievement_prerequisites_insert_sql)
    local rewardsins = static.db:prepare(achievement_rewards_insert_sql)
    local bitsins = static.db:prepare(achievement_bits_insert_sql)

    local requests = 0
    local lastreqbucket = overlay.time()

    for i, bucket in ipairs(idbuckets) do
        local idsstr = table.concat(bucket, ',')
        local r,achievements = api.get('achievements', {ids = idsstr}, nil, nil, nil, false)

        if not r then
            error("Couldn't fetch achievement details")
        end

        for i, achievement in ipairs(achievements) do
            achievementins:bind(':id', achievement.id)
            achievementins:bind(':icon', achievement.icon)
            achievementins:bind(':name', achievement.name)
            achievementins:bind(':description', achievement.description)
            achievementins:bind(':requirement', achievement.requirement)
            achievementins:bind(':locked_text', achievement.locked_text)
            achievementins:bind(':type', achievement.type)
            achievementins:bind(':point_cap', achievement.point_cap)
            achievementins:step()
            achievementins:reset()
            
            if achievement.flags then
                for i,flag in ipairs(achievement.flags) do
                    flagsins:bind(':achievement', achievement.id)
                    flagsins:bind(':flag', flag)
                    flagsins:step()
                    flagsins:reset()
                end
            end

            if achievement.tiers then
                for i,tier in ipairs(achievement.tiers) do
                    tiersins:bind(':achievement', achievement.id)
                    tiersins:bind(':count', tier.count)
                    tiersins:bind(':points', tier.points)
                    tiersins:bind(':sequence', i)
                    tiersins:step()
                    tiersins:reset()
                end
            end

            if achievement.prerequisites then
                for i,prereq in ipairs(achievement.prerequisites) do
                    prereqsins:bind(':achievement', achievement.id)
                    prereqsins:bind(':prerequisite', prereq)
                    prereqsins:step()
                    prereqsins:reset()
                end
            end

            if achievement.rewards then
                for i,reward in ipairs(achievement.rewards) do
                    rewardsins:bind(':achievement', achievement.id)
                    rewardsins:bind(':type', reward.type)
                    rewardsins:bind(':count', reward.count)
                    rewardsins:bind(':id', reward.id)
                    rewardsins:bind(':region', reward.region)
                    rewardsins:step()
                    rewardsins:reset()
                end
            end

            if achievement.bits then
                for i,bit in ipairs(achievement.bits) do
                    bitsins:bind(':achievement', achievement.id)
                    bitsins:bind(':sequence', i)
                    bitsins:bind(':type', bit.type)
                    bitsins:bind(':id', bit.id)
                    bitsins:bind(':text', bit.text)
                    bitsins:step()
                    bitsins:reset()
                end
            end
        end

        requests = requests + 1
        if requests > 5 then
            while overlay.time() - lastreqbucket < 1 do
                coroutine.yield()
            end
            lastreqbucket = overlay.time()
            requests = 0
        end
    end

    static.log:info('Achievement data update complete.')
end

local function runupdatespecs()
    static.log:info("Updating specializations data...")

    local r,specs = api.get('specializations', {ids="all"}, nil, nil, nil, false)

    if not r then
        error("Couldn't fetch specializations.")
    end

    local specins = static.db:prepare(specializations_insert_sql)
    local spectraitins = static.db:prepare(spec_traits_insert_sql)

    static.db:execute('DELETE FROM specializations')
    static.db:execute('DELETE FROM specialization_traits')

    for i, spec in ipairs(specs) do
        specins:bind(':id'          , spec.id)
        specins:bind(':name'        , spec.name)
        specins:bind(':profession'  , spec.profession)
        specins:bind(':elite'       , spec.elite)
        specins:bind(':weapon_trait', spec.weapon_trait)
        specins:bind(':icon'        , spec.icon)
        specins:bind(':background'  , spec.background)
        specins:step()
        specins:reset()

        for i,trait in ipairs(spec.minor_traits) do
            spectraitins:bind(':id'            , trait)
            spectraitins:bind(':specialization', spec.id)
            spectraitins:bind(':major'         , false)
            spectraitins:step()
            spectraitins:reset()
        end

        for i,trait in ipairs(spec.major_traits) do
            spectraitins:bind(':id'            , trait)
            spectraitins:bind(':specialization', spec.id)
            spectraitins:bind(':major'         , true)
            spectraitins:step()
            spectraitins:reset()
        end
    end

    static.log:info("Specializations data update complete.")
end

local function runupdatecontinents()
    static.log:info("Updating continents data...")

    local r,conts = api.get('continents')    

    if not r then
        error("Couldn't fetch continents, aborting udate.")
    end

    local contins = static.db:prepare(continent_insert_sql)
    local regionins = static.db:prepare(region_insert_sql)
    local mapins = static.db:prepare(map_insert_sql)
    local poiins = static.db:prepare(poi_insert_sql)
    local taskins = static.db:prepare(task_insert_sql)
    local taskboundsins = static.db:prepare(taskbounds_insert_sql)
    local scins = static.db:prepare(skillchallenge_insert_sql)
    local sectorins = static.db:prepare(sector_insert_sql)
    local sectorboundsins = static.db:prepare(sectorbounds_insert_sql)
    local advins = static.db:prepare(adventures_insert_sql)
    local mpins = static.db:prepare(masterypoints_insert_sql)

    static.db:execute('DELETE FROM continents')
    static.db:execute('DELETE FROM regions')
    static.db:execute('DELETE FROM maps')
    static.db:execute('DELETE FROM pois')
    static.db:execute('DELETE FROM tasks')
    static.db:execute('DELETE FROM taskbounds')
    static.db:execute('DELETE FROM skillchallenges')
    static.db:execute('DELETE FROM sectors')
    static.db:execute('DELETE FROM sectorbounds')
    static.db:execute('DELETE FROM adventures')
    static.db:execute('DELETE FROM masterypoints')

    local mapids = {}
    local regionids = {}

    for i, continentid in ipairs(conts) do
        local r,contdata = api.get(string.format('continents/%d', continentid), nil, nil, nil, nil, false)        

        local floors = {}
        for i,f in ipairs(contdata.floors) do table.insert(floors,math.tointeger(f)) end

        contins:bind(':id', contdata.id)
        contins:bind(':name', contdata.name)
        contins:bind(':continent_width', contdata.continent_dims[1])
        contins:bind(':continent_height', contdata.continent_dims[2])
        contins:bind(':min_zoom', contdata.min_zoom)
        contins:bind(':max_zoom', contdata.max_zoom)
        contins:bind(':floors', table.concat(floors,','))
        contins:step()
        contins:reset()

        for i,floor in ipairs(floors) do
            -- don't cache these responses, they are pretty large as parsed responses (~100MB after all done here)
            local r, floordata = api.get(string.format('continents/%d/floors/%d', continentid, floor), nil, nil, nil, nil, false)
            for regionid, regiondata in pairs(floordata.regions) do
                if not regionids[regionid] then 
                    regionins:bind(':id', regionid)
                    regionins:bind(':name', regiondata.name)
                    regionins:bind(':continent', continentid)
                    regionins:bind(':continent_rect_left'  , regiondata.continent_rect[1][1])
                    regionins:bind(':continent_rect_right' , regiondata.continent_rect[2][1])
                    regionins:bind(':continent_rect_top'   , regiondata.continent_rect[2][2])
                    regionins:bind(':continent_rect_bottom', regiondata.continent_rect[1][2])
                    regionins:step()
                    regionins:reset()

                    regionids[regionid] = true
                end

                for mapid, mapdata in pairs(regiondata.maps) do
                    if mapids[mapid] then
                        --static.log:warn("Duplicate map id %d (%s), skipping.", mapid, mapdata.name)
                        goto continuemaps
                    end

                    mapins:bind(':id', mapid)
                    mapins:bind(':region', regionid)
                    mapins:bind(':name', mapdata.name)
                    mapins:bind(':min_level', mapdata.min_level)
                    mapins:bind(':max_level', mapdata.max_level)
                    mapins:bind(':default_floor', mapdata.default_floor)
                    if mapdata.label_coord then
                        mapins:bind(':label_x', mapdata.label_coord[1])
                        mapins:bind(':label_y', mapdata.label_coord[2])
                    else
                        mapins:bind(':label_x', nil)
                        mapins:bind(':label_y', nil)
                        --static.log:warn("Map %s (%d) does not have label_coord.", mapdata.name, mapid)
                    end
                    mapins:bind(':map_rect_left'  , mapdata.map_rect[1][1])
                    mapins:bind(':map_rect_right' , mapdata.map_rect[2][1])
                    mapins:bind(':map_rect_top'   , mapdata.map_rect[2][2])
                    mapins:bind(':map_rect_bottom', mapdata.map_rect[1][2])
                    mapins:bind(':continent_rect_left'  , mapdata.continent_rect[1][1])
                    mapins:bind(':continent_rect_right' , mapdata.continent_rect[2][1])
                    mapins:bind(':continent_rect_top'   , mapdata.continent_rect[2][2])
                    mapins:bind(':continent_rect_bottom', mapdata.continent_rect[1][2])
                    mapins:step()
                    mapins:reset()

                    for poiid, poidata in pairs(mapdata.points_of_interest) do
                        poiins:bind(':id', poiid)
                        poiins:bind(':name', poidata.name)
                        poiins:bind(':type', poidata.type)
                        poiins:bind(':floor', poidata.floor)
                        poiins:bind(':x', poidata.coord[1])
                        poiins:bind(':y', poidata.coord[2])
                        poiins:bind(':chat_link', poidata.chat_link)
                        poiins:bind(':icon', poidata.icon)
                        poiins:bind(':map', mapid)
                        poiins:step()
                        poiins:reset()
                    end

                    for taskid, taskdata in pairs(mapdata.tasks) do
                        taskins:bind(':id', taskid)
                        taskins:bind(':objective', taskdata.objective)
                        taskins:bind(':level', taskdata.level)
                        taskins:bind(':x', taskdata.coord[1])
                        taskins:bind(':y', taskdata.coord[2])
                        taskins:bind(':chat_link', taskdata.chat_link)
                        taskins:bind(':map', mapid)
                        taskins:step()
                        taskins:reset()

                        for i,boundcoord in ipairs(taskdata.bounds) do
                            taskboundsins:bind(':seq', i)
                            taskboundsins:bind(':task', taskid)
                            taskboundsins:bind(':x', boundcoord[1])
                            taskboundsins:bind(':y', boundcoord[2])
                            taskboundsins:step()
                            taskboundsins:reset()
                        end
                    end

                    for i, scdata in ipairs(mapdata.skill_challenges) do
                        scins:bind(':id', scdata.id)
                        scins:bind(':x', scdata.coord[1])
                        scins:bind(':y', scdata.coord[2])
                        scins:bind(':map', mapid)
                        scins:step()
                        scins:reset()
                    end

                    for secid, secdata in pairs(mapdata.sectors) do
                        sectorins:bind(':id', secid)
                        sectorins:bind(':name', secdata.name)
                        sectorins:bind(':level', secdata.level)
                        sectorins:bind(':x', secdata.coord[1])
                        sectorins:bind(':y', secdata.coord[2])
                        sectorins:bind(':map', mapid)
                        sectorins:step()
                        sectorins:reset()

                        for i, boundcoord in ipairs(secdata.bounds) do
                            sectorboundsins:bind(':seq', i)
                            sectorboundsins:bind(':sector', secid)
                            sectorboundsins:bind(':map', mapid)
                            sectorboundsins:bind(':x', boundcoord[1])
                            sectorboundsins:bind(':y', boundcoord[2])
                            sectorboundsins:step()
                            sectorboundsins:reset()
                        end
                    end

                    for i, advdata in ipairs(mapdata.adventures) do
                        advins:bind(':id', advdata.id)
                        advins:bind(':name', advdata.name)
                        advins:bind(':x', advdata.coord[1])
                        advins:bind(':y', advdata.coord[2])
                        advins:bind(':description', advdata.description)
                        advins:bind(':map', mapid)
                        advins:step()
                        advins:reset()
                    end

                    for i, mpdata in ipairs(mapdata.mastery_points) do
                        mpins:bind(':id', mpdata.id)
                        mpins:bind(':x', mpdata.coord[1])
                        mpins:bind(':y', mpdata.coord[2])
                        mpins:bind(':region', mpdata.region)
                        mpins:bind(':map', mapid)
                        mpins:step()
                        mpins:reset()
                    end

                    mapids[mapid] = true
                    ::continuemaps::
                    coroutine.yield()
                end
            end
        end
    end

    static.log:info('Continents data update complete.')
end

--[[ RST
Functions
---------
]]--

--[[ RST
.. lua:function:: updateachievements()

    Update the following tables from the GW2 API.

    - achievements
    - achievement_flags
    - achievement_tiers
    - achievement_prerequisites
    - achievement_rewards
    - achievement_bits

    .. versionhistory::
        :0.0.1: Added
]]--
function static.updateachievements()
    static.db:execute('BEGIN')

    local r,msg = xpcall(runupdateachievements, function(msg) return debug.traceback(msg, 2) end)
    if not r then
        static.log:error('Error during updating achievement data, rolling back database:\n%s', msg)
        static.db:execute('ROLLBACK')
    else
        static.db:execute('COMMIT')
    end
end

--[[ RST
.. lua:function:: updatespecializations()

    Update the following tables from the GW2 API.

    - specializations
    - specialization_traits

    .. versionhistory::
        :0.0.1: Added
]]--
function static.updatespecializations()
    static.db:execute('BEGIN')

    local r,msg = xpcall(runupdatespecs, function(msg) return debug.traceback(msg, 2) end)
    if not r then
        static.log:error('Error during updating specialization data, rolling back database:\n%s', msg)
        static.db:execute('ROLLBACK')
    else
        static.db:execute('COMMIT')
    end
end

--[[ RST
.. lua:function:: updatecontinents()

    Update the following tables from the GW2 API. If an error occurs, no changes will be made.

    - continents
    - regions
    - maps
    - pois (waypoints, vistas, pois, etc.)
    - tasks (renown hearts)
    - taskbounds
    - skillchallenges (hero points)
    - sectors
    - sectorbounds
    - adventures
    - masterypoints

    .. versionhistory::
        :0.0.1: Added
]]--
function static.updatecontinents()
    static.db:execute('BEGIN')
    
    local r,msg = xpcall(runupdatecontinents, function(msg) return debug.traceback(msg, 2) end)
    if not r then 
        static.log:error("Error during updating continent data, rolling back databse:\n%s", msg)
        static.db:execute('ROLLBACK')
    else
        static.db:execute('COMMIT')
    end
end

--[[ RST
.. lua:function:: specialization(specid)

    Return the specializtion given by ``specid``.

    The returned value is a Lua table with the following fields.

    .. luatablefields::
        :id: Specialization ID
        :name: Specialization Name
        :profession: Profession Name
        :elite: Indicates if this is an elite specialization
        :icon: Icon URL
        :background: Background URL
        :minor_traits: A sequence of minor trait IDs
        :major_traits: A sequence of major trait IDs

    .. versionhistory::
        :0.0.1: Added
]]--
function static.specialization(specid)
    local s = static.db:prepare("SELECT * FROM specializations WHERE id = ?")
    s:bind(1, specid)

    local spec = s:step()

    s:finalize()

    if spec then
        spec.major_traits = {}
        spec.minor_traits = {}
        s = static.db:prepare("SELECT id, major FROM specialization_traits WHERE specialization = ?")
        local function rows()
            return s:step()
        end

        for row in rows do
            if row.major > 0 then
                table.insert(spec.major_traits, row.id)
            else
                table.insert(spec.minor_traits, row.id)
            end
        end

        return spec
    end
end

--[[ RST
.. lua:function:: waypointsinmap(mapid)

    Return a table of waypoints for the given map ID.

    :param mapid: The Map ID
    :type mapid: integer
    :returns: A table of waypoints. See :overlay:dbtable:`gw2static.pois`.
    :rtype: table

    .. versionhistory::
        :0.0.1: Added
]]--
function static.waypointsinmap(mapid)    
    local s = static.db:prepare("SELECT * FROM pois WHERE pois.map = ? AND pois.type = 'waypoint'")

    s:bind(1, mapid)

    local wps = {}

    local function rows()
        return s:step()
    end

    for row in rows do
        table.insert(wps, row)
    end

    s:finalize()

    return wps
end

function static.map(mapid)
    local s = static.db:prepare("SELECT * FROM maps WHERE id = ?")
    s:bind(1, mapid)
    return s:step()
end


return static

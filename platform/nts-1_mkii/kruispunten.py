import requests
import pandas as pd
import time

def get_intersections_beekdaelen():
    print("⏳ Data ophalen bij OpenStreetMap voor Beekdaelen... (dit kan even duren)")

    # De Overpass Query specifiek voor Beekdaelen
    # We zoeken naar Admin Level 8 (Gemeente) en daarbinnen naar wegen voor auto's.
    overpass_query = """
    [out:json];
    area["name"="Beekdaelen"]["admin_level"="8"]->.searchArea;
    (
      way(area.searchArea)["highway"~"^(primary|secondary|tertiary|residential|unclassified)$"];
    );
    (._;>;);
    out body;
    """

    response = requests.get("http://overpass-api.de/api/interpreter", params={'data': overpass_query})
    
    if response.status_code != 200:
        print(f"❌ Fout bij ophalen data: {response.status_code}")
        return

    data = response.json()
    print(f"✅ Data binnen! {len(data['elements'])} elementen verwerken...")

    # Data structuren
    nodes_coords = {}     # ID -> (Lat, Lon)
    node_usage = {}       # NodeID -> Set van straatnamen
    
    # Stap 1: Eerst alle nodes (punten) opslaan met hun coördinaten
    for element in data['elements']:
        if element['type'] == 'node':
            nodes_coords[element['id']] = (element['lat'], element['lon'])

    # Stap 2: Door alle wegen (ways) lopen en kijken welke nodes ze gebruiken
    for element in data['elements']:
        if element['type'] == 'way':
            # We pakken de straatnaam, of 'Onbekende weg' als die er niet is
            street_name = element.get('tags', {}).get('name', 'Naamloze weg')
            
            # Loop door alle nodes van deze weg
            for node_id in element.get('nodes', []):
                if node_id not in node_usage:
                    node_usage[node_id] = set()
                node_usage[node_id].add(street_name)

    # Stap 3: Filteren op kruispunten
    # Een node is een kruispunt als hij voorkomt in data van MEER dan 1 unieke weg-entiteit.
    # Echter, OSM knipt wegen soms op zonder dat het een kruispunt is. 
    # Een betere filter voor een app is: komen er minimaal 2 *verschillende* straatnamen samen?
    # Of wil je ook T-splitsingen van dezelfde straat? 
    # Hieronder filteren we op: Node wordt gedeeld door minstens 2 stukken weg.
    
    intersections = []
    
    for node_id, streets in node_usage.items():
        # Filter: Het is een kruispunt als er 2 of meer straatnamen aan verbonden zijn
        # OF als de set minstens 2 items bevat (dus ook kruisingen van wegen met diverse ID's)
        # Voor de netheid pakken we nu nodes waar minstens 2 wegen samenkomen.
        
        # Omdat OSM wegen in stukjes hakt, filteren we hier op:
        # Minimaal 2 verschillende straatnamen (kruising)
        # OF meer dan 2 weg-segmenten (b.v. een 3-sprong van dezelfde straatnaam)
        
        # Voor dit voorbeeld houden we het simpel: 
        # Als er op 1 punt, 2 verschillende straatnamen samenkomen, is het zeker een kruising.
        if len(streets) >= 2:
            lat, lon = nodes_coords.get(node_id, (0, 0))
            intersections.append({
                'Node ID': node_id,
                'Latitude': lat,
                'Longitude': lon,
                'Wegen': ", ".join(list(streets))
            })

    # Stap 4: Exporteren
    print(f"🧩 {len(intersections)} kruispunten gevonden in Beekdaelen.")
    
    if intersections:
        df = pd.DataFrame(intersections)
        bestandsnaam = 'kruispunten_beekdaelen.xlsx'
        df.to_excel(bestandsnaam, index=False)
        print(f"💾 Bestand opgeslagen als: {bestandsnaam}")
        print("   Je kunt dit bestand nu inlezen in je app of database.")
    else:
        print("Geen kruispunten gevonden (check de query of filters).")

if __name__ == "__main__":
    get_intersections_beekdaelen()
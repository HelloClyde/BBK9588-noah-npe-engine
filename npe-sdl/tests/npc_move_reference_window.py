"""Use original commits through the map initialization, excluding later idle ticks."""

def through_map_initialization(row):
    if row.get('nonterminal'):
        return row
    end = next(i for i, c in enumerate(row['commits']) if c['state']['mode'] == 1)
    commit = row['commits'][end]
    assert commit['tick'] >= row['finish']
    # 37:1362 clears7AB4 before the original map-loading commit at37:1366.
    # Older extended samples lack this field; retain their actual pixels/states.
    assert not commit['state'].get('mapInit', 0)
    return dict(row, commits=row['commits'][:end + 1], after=commit['state'])

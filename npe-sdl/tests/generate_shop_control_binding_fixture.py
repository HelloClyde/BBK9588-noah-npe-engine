import json
from pathlib import Path
source=Path('npe-sdl/tests/generate_shop_item_cache_fixture.py').read_text(encoding='utf-8')
source=source.replace("selection_matrix='--selection' in sys.argv[1:]", "selection_matrix=False")
source=source.replace("prefix='native-shop-selection' if selection_matrix else 'native-shop-item-cache'", "prefix='native-shop-control-binding'")
source=source.replace("'descLength','inventoryPrefix']", "'descLength','inventoryPrefix','kind','sellFlag']")
source=source.replace('ShopItemFrame frame[48];}', 'ShopItemFrame frame[48];unsigned afterFlag,afterKind;}')
source=source.replace("for c in frames)+'}},')", "for c in frames)+'},'+str(r.get('afterFlag',256))+','+str(r.get('afterKind',256))+'},')")
source=source.replace("assert len(rows)==(8 if selection_matrix else 6)", "assert len(rows)==4")
source=source.replace("assert total==(41 if selection_matrix else 37)", "assert total==16")
source=source.replace("('shop_selection_native.h' if selection_matrix else 'shop_item_cache_native.h')", "'shop_control_binding_native.h'")
exec(compile(source, __file__, 'exec'))

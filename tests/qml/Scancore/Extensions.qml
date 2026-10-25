pragma Singleton
import QtQuick
QtObject { property int updateCount: 0; property int revision: 0; function installed(type) { return [] } }

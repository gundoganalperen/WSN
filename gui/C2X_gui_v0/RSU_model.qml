import QtQuick 2.0
import QtQuick.Shapes 1.11

Item {
    id: rsu
    width: 200
    height: 200
    Rectangle {
        id: rect
        x: 0
        y: 0
        width: 200
        height: 200
        color: "#b6d48f"
        radius: 87
        border.width: 0

        Image {
            id: image
            x: 12
            y: 12
            width: 180
            height: 180
            fillMode: Image.Stretch
            source: "../images/RSU_2.png"
        }
    }

}

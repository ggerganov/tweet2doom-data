require('browser-env')();

const vis = require('vis-network');

{
    console.log('reading nodes and edges ..');

    var fs = require('fs');
    eval(fs.readFileSync('../../public/js/graph.js') + '');

    console.log('done');
}

console.log('running graph_create() ..');

var nodes = [];
var edges = [];

graph_create();

console.log('done');

// filter depth 1 nodes without children
// there are simply too much of those ..
{
    console.log('filtering ..');

    var nodeMap = {};

    for (var i = 0; i < nodes.length; ++i) {
        nodeMap[nodes[i].id] = i;
        nodes[i].t2d_count = 0;
    }

    for (var i = 0; i < edges.length; ++i) {
        nodes[nodeMap[edges[i].to]].t2d_count++;
    }

    var nodesFiltered = [];

    for (var i = 0; i < edges.length; ++i) {
        if (nodes[nodeMap[edges[i].from]].t2d_count == 0 && nodes[nodeMap[edges[i].from]].level == 2) {
            nodes[nodeMap[edges[i].to]].t2d_count = -1;
        }
    }

    for (var i = 0; i < nodes.length; ++i) {
        if (nodes[i].t2d_count > 0 || nodes[i].level > 2) {
            nodesFiltered.push(nodes[i]);
        }
    }

    nodes = nodesFiltered;

    console.log('done');
}

// create a network
//var container = window.document.getElementById("mynetwork");
var container = window.document.createElement('mynetwork');

var data = {
    nodes: nodes,
    edges: edges,
};

var options = {
    layout: {
        hierarchical: {
            direction: "UD",
            sortMethod: "directed",
            nodeSpacing: 140,
            edgeMinimization: true,
            parentCentralization: true,
        },
    },
    physics: false,
    groups: {
        node: {
            shape: "box",
            color: "#ff0000",
        },
        command: {
            shape: "box",
            color: "#00ff00",
        },
    },
};

console.log('generating ...');

var network = new vis.Network(container, data, options);
var pos = network.getPositions();

// output json data
{
    console.log('writing "nodes.json" ...');
    fs.writeFileSync("../../public/json/nodes.json", JSON.stringify(nodes));
    console.log('writing "edges.json" ...');
    fs.writeFileSync("../../public/json/edges.json", JSON.stringify(edges));
    console.log('writing "positions.json" ...');
    fs.writeFileSync("../../public/json/positions.json", JSON.stringify(pos));
}

// output nodes
{
    var res = '';
    for (var idx in nodes) {
        const node = nodes[idx];
        res += node.id + ' ' + node.label + ' ' + node.level + ' ' + (node.group == 'root' ? '0' : node.group == 'node' ? '1' : '2') + '\n';
    }

    console.log('writing "nodes.dat" ...');
    fs.writeFileSync("../../data/nodes.dat", res);
    console.log('done');
}

// output edges
{
    var res = '';
    for (var idx in edges) {
        const edge = edges[idx];
        res += edge.from + ' ' + edge.to + '\n';
    }

    console.log('writing "edges.dat" ...');
    fs.writeFileSync("../../data/edges.dat", res);
    console.log('done');
}

// output coordinates
{
    var res = '';
    for (var id in pos) {
        res += id + ' ' + pos[id].x + ' ' + pos[id].y + '\n';
    }

    console.log('writing "coordinates.dat" ...');
    fs.writeFileSync("../../data/coordinates.dat", res);
    console.log('done');
}

process.exit(0);
